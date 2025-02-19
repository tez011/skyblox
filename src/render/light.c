#include "render.h"
#include "world.h"

static const int cascade_planes[SUN_SHADOW_CASCADES] = { 10, 30, 60, 100 };

int buffer_light_data(int cx, int cy)
{
	int total_lights = 0, max_lights_in_chunk = 0, light_radius = chunk_render_radius + 1, lb_offset = 0;
	for (int rx = -light_radius; rx <= light_radius; rx++) {
		for (int ry = -light_radius; ry <= light_radius; ry++) {
			chunk_t *ch = chunks_get(cx + rx, cy + ry);
			if (ch != NULL) {
				total_lights += ch->num_lights;
				max_lights_in_chunk = MAX(max_lights_in_chunk, ch->num_lights);
			}
		}
	}

	mat4 *lb = malloc(max_lights_in_chunk * sizeof(mat4));
	glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_LIGHTPROPS]);
	glBufferData(GL_ARRAY_BUFFER, total_lights * sizeof(mat4), NULL, GL_DYNAMIC_DRAW);
	for (int rx = -light_radius; rx <= light_radius; rx++) {
		for (int ry = -light_radius; ry <= light_radius; ry++) {
			chunk_t *ch = chunks_get(cx + rx, cy + ry);
			if (ch == NULL || ch->num_lights == 0)
				continue;

			memcpy(lb, ch->light_data, ch->num_lights * sizeof(mat4));
			for (int i = 0; i < ch->num_lights; i++) {
				lb[i][0][0] += 0.5f + (cx + rx) * CHUNK_WIDTH;
				lb[i][0][1] += 0.5f + (cy + ry) * CHUNK_WIDTH;
				lb[i][0][2] += 0.5f;
			}

			glBufferSubData(GL_ARRAY_BUFFER, lb_offset, ch->num_lights * sizeof(mat4), lb);
			lb_offset += ch->num_lights * sizeof(mat4);
		}
	}

	free(lb);
	return total_lights;
}

void draw_skyshadow_maps(int cx, int cy, vec4 *vf_corners, mat4 *lightspace, float *proj_cascade_planes)
{
	vec4 subvf_corners[8] = { 0 };
	mat4 lv,lp;
	use_shader(shaders[SHADER_DEPTHRENDER]);
	glm_lookat(igdt.sun, GLM_VEC4_ZERO, GLM_ZUP, lv);
	glViewport(0, 0, SUN_SHADOW_SIZE, SUN_SHADOW_SIZE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	for (int i = 0; i < SUN_SHADOW_CASCADES; i++) {
		vec3 lp_bb[2];
		/* Load subvf_corners with the corners of the cascade.
		 * The first cascade's near plane is the near plane of the view frustum, and
		 * the last cascade's far plane is the far plane of the view frustum.
		 * All other near planes are the far planes of the last cascade. */
		if (i == 0)
			memcpy(subvf_corners, vf_corners, 4 * sizeof(vec4));
		else
			memcpy(subvf_corners, subvf_corners + 4, 4 * sizeof(vec4));
		if (i == SUN_SHADOW_CASCADES - 1)
			memcpy(subvf_corners + 4, vf_corners + 4, 4 * sizeof(vec4));
		else
			glm_frustum_corners_at(vf_corners, cascade_planes[i], RENDER_FAR, subvf_corners + 4);

		/* Calculate the bounding box of the sub-frustum and make an
		 * orthographic projection matrix from it. Add 10 blocks' padding. */
		glm_frustum_box(subvf_corners, lv, lp_bb);
		glm_ortho_aabb_p(lp_bb, 10.f, lp);
		glm_mat4_mul(lp, lv, lightspace[i]);

		glUniformMatrix4fv(get_shader_uniform(0, "lightspace"), 1, GL_FALSE, *lightspace[i]);
		glUniformMatrix4fv(get_shader_uniform(0, "model"), 1, GL_FALSE, *GLM_MAT4_IDENTITY);
		glBindFramebuffer(GL_FRAMEBUFFER, sun_shadow[i]);
		glClear(GL_DEPTH_BUFFER_BIT);
		render_chunk_buffers(cx, cy, VBUF_BLOCKS, NULL);
	}
	glCullFace(GL_BACK);
	glViewport(0, 0, g_screen_width, g_screen_height);

	/* Multiply the cascade planes' values by an arbitrary light projection matrix,
	 * effectively projecting the cascade limits into light space. We can use this in conjunction
	 * with the calculated fragment depth to determine which cascade to use for shadow mapping.
	 * In this case, the LP matrix used is for the final cascade. */
	for (int i = 0; i < SUN_SHADOW_CASCADES; i++) {
		double z = 2.0 * RENDER_NEAR * RENDER_FAR;
		z -= cascade_planes[i] * (RENDER_NEAR + RENDER_FAR);
		z /= cascade_planes[i] * (RENDER_NEAR - RENDER_FAR);
		proj_cascade_planes[i] = (z / 2.f) + 0.5f;
	}
}

void draw_pointlights(mat4 vp, int num_lights)
{
	glDepthMask(GL_FALSE);
	glEnable(GL_STENCIL_TEST);
	for (int lpass = 0; lpass < DEPTH_PEEL_PASSES * 2; lpass++) {
		int pass = lpass / 2;
		bool stencil_pass = lpass % 2 == 0, light_pass = !stencil_pass;
		if (stencil_pass)
			use_shader(shaders[SHADER_LIGHTSTENCIL]);
		else
			use_shader(shaders[SHADER_LIGHTVOLUME]);

		glBindFramebuffer(GL_FRAMEBUFFER, GBUF(pass, GBUF_LIGHTING_FBUF));
		glUniformMatrix4fv(get_shader_uniform(0, "vp"), 1, GL_FALSE, *vp);
		if (light_pass) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_POSITION));
			glUniform1i(get_shader_uniform(0, "gbuf_position"), 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_NORMAL));
			glUniform1i(get_shader_uniform(0, "gbuf_normal"), 1);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_SPECULAR));
			glUniform1i(get_shader_uniform(0, "gbuf_specular_shininess"), 2);
			glUniform2f(get_shader_uniform(0, "fbdims"), g_screen_width, g_screen_height);
			glUniform3f(get_shader_uniform(0, "view_pos"), igdt.loc[0], igdt.loc[1], igdt.loc[2]);
		}

		if (stencil_pass) {
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

			glClear(GL_STENCIL_BUFFER_BIT);
			glStencilFunc(GL_ALWAYS, 0, 0xFF);
			glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
			glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
		} else if (light_pass) {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);

			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_ONE, GL_ONE);
		}

		GLint vertex = get_shader_attrib(0, "vertex"), light_pos_size = get_shader_attrib(0, "light_pos_size"), light_color,
		      light_strength;
		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_LIGHTPROPS]);
		glVertexAttribPointer(light_pos_size, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(0));
		glVertexAttribDivisor(light_pos_size, 1);
		glEnableVertexAttribArray(light_pos_size);
		if (light_pass) {
			light_color = get_shader_attrib(0, "light_color");
			light_strength = get_shader_attrib(0, "light_strength");
			glVertexAttribPointer(light_color, 3, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(sizeof(vec4)));
			glVertexAttribPointer(light_strength, 3, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(2 * sizeof(vec4)));
			glVertexAttribDivisor(light_color, 1);
			glVertexAttribDivisor(light_strength, 1);
			glEnableVertexAttribArray(light_color);
			glEnableVertexAttribArray(light_strength);
		}

		glBindBuffer(GL_ARRAY_BUFFER, vbo[VBO_LIGHTVOL_SPHERE]);
		glEnableVertexAttribArray(vertex);
		glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, 0, (void *)(0));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[IBO_LIGHTVOL_SPHERE]);
		glDrawElementsInstanced(GL_TRIANGLES, 8 * 7 * 6, GL_UNSIGNED_BYTE, 0, num_lights);

		glDisableVertexAttribArray(vertex);
		glVertexAttribDivisor(light_pos_size, 0);
		glDisableVertexAttribArray(light_pos_size);
		if (light_pass) {
			glVertexAttribDivisor(light_color, 0);
			glVertexAttribDivisor(light_strength, 0);
			glDisableVertexAttribArray(light_color);
			glDisableVertexAttribArray(light_strength);

			glCullFace(GL_BACK);
			glDisable(GL_BLEND);
		}
	}
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

void draw_skylights(mat4 *lightspace, float *proj_cascade_planes)
{
	use_shader(shaders[SHADER_SKYLIGHT]);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	glUniform2f(get_shader_uniform(0, "fbdims"), g_screen_width, g_screen_height);
	glUniform3f(get_shader_uniform(0, "view_pos"), igdt.loc[0], igdt.loc[1], igdt.loc[2]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, stextures[STEXTURE_SKY_SCATTERING]);
	glUniform1i(get_shader_uniform(0, "scattering_texture"), 0);
	for (int pass = 0; pass < DEPTH_PEEL_PASSES; pass++) {
		glBindFramebuffer(GL_FRAMEBUFFER, GBUF(pass, GBUF_LIGHTING_FBUF));
		glUniform1f(get_shader_uniform(0, "light_strength"), 1);
		glUniform3fv(get_shader_uniform(0, "skylight"), 1, igdt.sun);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_POSITION));
		glUniform1i(get_shader_uniform(0, "gbuf_position"), 1);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_NORMAL));
		glUniform1i(get_shader_uniform(0, "gbuf_normal"), 2);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_SPECULAR));
		glUniform1i(get_shader_uniform(0, "gbuf_specular_shininess"), 3);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, GBUF(pass, GBUF_DEPTH_BUFFER));
		glUniform1i(get_shader_uniform(0, "gbuf_depth"), 4);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D_ARRAY, sun_shadow[SUN_SHADOW_CASCADES]);
		glUniform1i(get_shader_uniform(0, "shadowmaps"), 5);
		glUniform1fv(get_shader_uniform(0, "cascade_planes"), SUN_SHADOW_CASCADES, proj_cascade_planes);
		glUniformMatrix4fv(get_shader_uniform(0, "lightspace"), SUN_SHADOW_CASCADES, GL_FALSE, **lightspace);
		glDrawArrays(GL_TRIANGLES, 0, 4);
	}
}
