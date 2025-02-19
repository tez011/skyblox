#include <assert.h>
#include "render.h"
#include "util.h"
#include "world.h"

int g_screen_width = INITIAL_SCREEN_WIDTH, g_screen_height = INITIAL_SCREEN_HEIGHT;
static int player_fov = 80;

static inline void calculate_view_matrix(double loc[3], double pitch, double yaw, mat4 view)
{
	vec3 position, look;
	for (int i = 0; i < 3; i++)
		position[i] = loc[i] + (i == 2 ? PLAYER_EYE_HEIGHT : 0);
	orientation_from_angles(look, pitch, yaw);
	glm_look(position, look, GLM_ZUP, view);
}

static void update_player_viewangle(SDL_Window *main_window)
{
	const double mouse_speed = 4;
	if (SDL_GetWindowFlags(main_window) & SDL_WINDOW_INPUT_FOCUS) {
		int mouse_x, mouse_y, dx, dy;
		double pitch = igdt.pitch, yaw = igdt.yaw;
		SDL_GetMouseState(&mouse_x, &mouse_y);
		dx = mouse_x - g_screen_width / 2;
		dy = mouse_y - g_screen_height / 2;

		yaw -= mouse_speed * dx / (g_screen_width / 2);
		yaw -= (2 * M_PI) * floor(yaw / (2 * M_PI));
		igdt.yaw = yaw > M_PI ? yaw - 2 * M_PI : yaw;

		pitch -= mouse_speed * dy / (g_screen_height / 2);
		igdt.pitch = MIN(M_PI / 2 - 0.01, MAX(-M_PI / 2 + 0.01, pitch));

		SDL_WarpMouseInWindow(main_window, g_screen_width / 2, g_screen_height / 2);
	}
}

static void draw_debug_info(struct nk_context *ui_ctx, int vw, int vh)
{
	static Uint32 last_frame_time = 0;
	Uint32 curr_frame_time = SDL_GetTicks();

	char plbuf[256];
	nk_style_push_color(ui_ctx, &ui_ctx->style.window.background, nk_rgba(0, 0, 0, 0));
	nk_style_push_style_item(ui_ctx, &ui_ctx->style.window.fixed_background, nk_style_item_color(nk_rgba(0, 0, 0, 0)));
	if (nk_begin(ui_ctx, "DEBUG_INFO_WIN", nk_rect(0, 0, vw, vh / 2), NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_dynamic(ui_ctx, 14, 1);

		sprintf(plbuf, "(%.3lf,%.3lf,%.3lf) pitch:%.3lf yaw:%.3lf dz:%.3lf frametime:%dms time:%d.%02d", igdt.loc[0], igdt.loc[1],
			igdt.loc[2], 180 * igdt.pitch / M_PI, 180 * igdt.yaw / M_PI, igdt.dz, curr_frame_time - last_frame_time,
			(int)igdt.time_of_day, (int)(60 * (igdt.time_of_day - ((int)igdt.time_of_day))));
		nk_label(ui_ctx, plbuf, NK_TEXT_LEFT);
		nk_end(ui_ctx);
	}
	nk_style_pop_color(ui_ctx);
	nk_style_pop_style_item(ui_ctx);
	last_frame_time = curr_frame_time;
}

void render_chunk_buffers(int cx, int cy, int vb, vec4 *vf_planes)
{
	GLuint vertex = get_shader_attrib(0, "vertex"), v_texture = get_shader_attrib(0, "v_texture");
	glEnableVertexAttribArray(vertex);
	if (v_texture != -1)
		glEnableVertexAttribArray(v_texture);

	for (int rx = -chunk_render_radius; rx <= chunk_render_radius; rx++) {
		for (int ry = -chunk_render_radius; ry <= chunk_render_radius; ry++) {
			chunk_t *chunk = chunks_get(cx + rx, cy + ry);
			if (chunk == NULL)
				continue;
			if (chunk->vbufsize[vb] == 0)
				continue;

			if (vf_planes != NULL && glm_aabb_frustum((vec3[]){ { (cx + rx) * CHUNK_WIDTH, (cy + ry) * CHUNK_WIDTH, 0 },
						       { (cx + rx + 1) * CHUNK_WIDTH, (cy + ry + 1) * CHUNK_WIDTH, CHUNK_HEIGHT } },
					     vf_planes) == false)
				continue;

			mat4 model;
			vec3 transl = { (cx + rx) * CHUNK_WIDTH, (cy + ry) * CHUNK_WIDTH, 0 };
			glm_translate_make(model, transl);
			glUniformMatrix4fv(get_shader_uniform(0, "model"), 1, GL_FALSE, *model);
			glBindBuffer(GL_ARRAY_BUFFER, chunk->vbuf[vb]);
			glVertexAttribPointer(vertex, 4, GL_FLOAT, GL_FALSE, VERTEX_DATA_SIZE * sizeof(float), (void *)0);
			glVertexAttribPointer(v_texture, 4, GL_FLOAT, GL_FALSE, VERTEX_DATA_SIZE * sizeof(float),
					      (void *)(4 * sizeof(float)));
			glDrawArrays(GL_TRIANGLES, 0, chunk->vbufsize[vb]);
		}
	}
	glDisableVertexAttribArray(vertex);
	if (v_texture != -1)
		glDisableVertexAttribArray(v_texture);
}

static void draw_chunks_geometry_pass(int cx, int cy, mat4 vp, vec4 vf_planes[6])
{
	use_shader(shaders[SHADER_BLOCKS]);
	glUniform2f(get_shader_uniform(0, "fbdims"), g_screen_width, g_screen_height);
	glUniformMatrix4fv(get_shader_uniform(0, "vp"), 1, GL_FALSE, *vp);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, block_textures);
	glUniform1i(get_shader_uniform(0, "block_textures"), 0);
	for (int pass = 0; pass < DEPTH_PEEL_PASSES; pass++) {
		glUniform1i(get_shader_uniform(0, "first_peel_pass"), pass == 0);

		/* bind the last depth buffer so we can discard fragments that are already part of another gbuffer */
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, pass == 0 ? 0 : GBUF(pass - 1, GBUF_DEPTH_BUFFER));
		glUniform1i(get_shader_uniform(0, "depth_texture"), 1);

		glBindFramebuffer(GL_FRAMEBUFFER, GBUF(pass, GBUF_GEOMETRY_FBUF));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_CULL_FACE);
		render_chunk_buffers(cx, cy, VBUF_BLOCKS, vf_planes);

		glDisable(GL_CULL_FACE);
		render_chunk_buffers(cx, cy, VBUF_TRANSLUCENT, vf_planes);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
}

static void draw_chunks(mat4 vp, mat4 projection, mat4 inv_vp)
{
	int cx, cy, num_lights;
	WORLD_CHUNK(igdt.loc[0], &cx, NULL);
	WORLD_CHUNK(igdt.loc[1], &cy, NULL);
	num_lights = buffer_light_data(cx, cy);

	vec4 vf_planes[6], vf_corners[8];
	glm_frustum_planes(vp, vf_planes);
	glm_frustum_corners(inv_vp, vf_corners);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);

	/******** Geometry Pass ********/
	draw_chunks_geometry_pass(cx, cy, vp, vf_planes);

	/******** Lighting Pass ********/
	/* Fill the depth buffers of the lighting buffers with opaque data. */
	for (int pass = 0; pass < DEPTH_PEEL_PASSES; pass++) {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GBUF(pass, GBUF_LIGHTING_FBUF));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		if (pass == 0) {
			glUniform1i(get_shader_uniform(0, "first_peel_pass"), 1);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glEnable(GL_CULL_FACE);
			render_chunk_buffers(cx, cy, VBUF_BLOCKS, vf_planes);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		} else {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, GBUF(0, GBUF_LIGHTING_FBUF));
			glBlitFramebuffer(0, 0, g_screen_width, g_screen_height, 0, 0, g_screen_width, g_screen_height, GL_DEPTH_BUFFER_BIT,
					  GL_NEAREST);
		}
	}

	/* Draw sun-shadow maps. */
	mat4 lightspace[SUN_SHADOW_CASCADES];
	float proj_cascade_planes[SUN_SHADOW_CASCADES];
	draw_skyshadow_maps(cx, cy, vf_corners, lightspace, proj_cascade_planes);

	/* Draw point lights. */
	draw_pointlights(vp, num_lights);

	/* Draw sky lights. */
	draw_skylights(lightspace, proj_cascade_planes);

	/******** Depth Peeling ********/
	use_shader(shaders[SHADER_COMBINE_GBUF]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_ALWAYS);
	glEnable(GL_BLEND);
	for (int pass = DEPTH_PEEL_PASSES - 1; pass >= 0; pass--) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_ALBEDO_TRANSPARENCY));
		glUniform1i(get_shader_uniform(0, "gp_albedo_transparency"), 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_SPECULAR));
		glUniform1i(get_shader_uniform(0, "gp_specular"), 1);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_AMBDIF_LIGHTING));
		glUniform1i(get_shader_uniform(0, "lp_ambdif"), 2);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_SPEC_LIGHTING));
		glUniform1i(get_shader_uniform(0, "lp_specular"), 3);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_DEPTH_BUFFER));
		glUniform1i(get_shader_uniform(0, "depth_buffer"), 4);

		glDrawArrays(GL_TRIANGLES, 0, 4);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, GBUF(pass, GBUF_GEOMETRY_FBUF));
		glBlitFramebuffer(0, 0, g_screen_width, g_screen_height, 0, 0, g_screen_width, g_screen_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	glDepthFunc(GL_LESS);
}

void render_sky(mat4 view, mat4 proj_inv)
{
	mat3 view_t;
	glm_mat4_pick3(view, view_t);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	use_shader(shaders[SHADER_SKY]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, stextures[STEXTURE_SKY_SCATTERING]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, stextures[STEXTURE_NIGHT_SKY]);
	glUniform1i(get_shader_uniform(0, "scattering_texture"), 0);
	glUniform1i(get_shader_uniform(0, "night_sky_texture"), 1);
	glUniform3fv(get_shader_uniform(0, "sun"), 1, igdt.sun);
	glUniform3fv(get_shader_uniform(0, "moon"), 1, igdt.moon);
	glUniformMatrix3fv(get_shader_uniform(0, "V_t"), 1, GL_TRUE, *view_t); /* let the GL driver transpose the matrix */
	glUniformMatrix4fv(get_shader_uniform(0, "P_inv"), 1, GL_FALSE, *proj_inv);

	glDepthMask(GL_FALSE);
	glDrawArrays(GL_TRIANGLES, 0, 4);
	glDepthMask(GL_TRUE);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static void draw_picked_block(mat4 vp)
{
	int verts = render_one_block(igdt.picked_block[0], igdt.picked_block[1], igdt.picked_block[2], false, vbo[VBO_BLOCKPICK]);
	vec3 transl = { igdt.picked_block[0] - 0.005f, igdt.picked_block[1] - 0.005f, igdt.picked_block[2] - 0.005f };
	mat4 model = GLM_MAT4_IDENTITY_INIT;
	glm_translate(model, transl);
	glm_scale_uni(model, 1.01f);

	use_shader(shaders[SHADER_BLOCKPICK]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUniformMatrix4fv(get_shader_uniform(0, "model"), 1, GL_FALSE, *model);
	glUniformMatrix4fv(get_shader_uniform(0, "vp"), 1, GL_FALSE, *vp);

	GLuint vertex = get_shader_attrib(0, "vertex"), v_texture = get_shader_attrib(0, "v_texture");
	glEnableVertexAttribArray(vertex);
	glEnableVertexAttribArray(v_texture);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_BLOCKPICK]);
	glVertexAttribPointer(vertex, 4, GL_FLOAT, GL_FALSE, VERTEX_DATA_SIZE * sizeof(float), (void *)(0));
	glVertexAttribPointer(v_texture, 3, GL_FLOAT, GL_FALSE, VERTEX_DATA_SIZE * sizeof(float), (void *)(4 * sizeof(float)));
	glDrawArrays(GL_TRIANGLES, 0, verts);

	glDisableVertexAttribArray(vertex);
	glDisableVertexAttribArray(v_texture);
}

void render_viewport_change(int width, int height)
{
	for (int pass = 0; pass < DEPTH_PEEL_PASSES; pass++) {
		for (int j = 0; j < GBUF_LAST_TEXTURE; j++) {
			glBindTexture(GL_TEXTURE_2D, GBUF(pass, j));
			render_allocate_gbuffer_textures(width, height, j);
		}

		glBindRenderbuffer(GL_RENDERBUFFER, GBUF(pass, GBUF_LIGHTING_DEPTH_RBUF));
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	}
}

void render_main(SDL_Window *window, struct nk_context *ui_ctx)
{
	SDL_GetWindowSize(window, &g_screen_width, &g_screen_height);
	update_player_viewangle(window);

	mat4 view, projection, vp, inv_proj, inv_vp;
	calculate_view_matrix(igdt.loc, igdt.pitch, igdt.yaw, view);
	glm_perspective(glm_rad(player_fov), (float)g_screen_width / (float)g_screen_height, RENDER_NEAR, RENDER_FAR, projection);
	glm_mat4_mul(projection, view, vp);
	glm_mat4_inv(projection, inv_proj);
	glm_mat4_inv(vp, inv_vp);

	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glBindVertexArray(vao);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_screen_width, g_screen_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	render_sky(view, inv_proj);
	draw_chunks(vp, projection, inv_vp);

	if (igdt.picked_block_face != FACE_UNKNOWN)
		draw_picked_block(vp);

	draw_debug_info(ui_ctx, g_screen_width, g_screen_height);
	nk_sdl_render(NK_ANTI_ALIASING_ON, 1 << 19, 1 << 17);
	SDL_GL_SwapWindow(window);
}
