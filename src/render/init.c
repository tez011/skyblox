#include "render.h"

GLuint vao, vbo[VBO_MAX], shaders[SHADER_MAX], stextures[STEXTURE_MAX];
GLuint gbuffer[DEPTH_PEEL_PASSES * GBUF_FBIDX_MAX];
GLuint sun_shadow[SUN_SHADOW_CASCADES + 1];

float sphere_verts[10 * 8 * 3];
GLubyte sphere_index[10 * 8 * 6];

void render_allocate_gbuffer_textures(int width, int height, int buffer_slice)
{
	switch (buffer_slice) {
	case GBUF_POSITION:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
		break;
	case GBUF_NORMAL:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
		break;
	case GBUF_ALBEDO_TRANSPARENCY:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		break;
	case GBUF_AMBDIF_LIGHTING:
	case GBUF_SPEC_LIGHTING:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
		break;
	case GBUF_DEPTH_BUFFER:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		break;
	case GBUF_SPECULAR:
	default:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		break;
	}
}

static void render_initialize_static_textures(void)
{
	glGenTextures(STEXTURE_MAX, stextures);
	size_t slen;

	void *scattering_data = read_physfs_file("/resources/textures/scattering.dat", &slen);
	glBindTexture(GL_TEXTURE_3D, stextures[STEXTURE_SKY_SCATTERING]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, 256, 128, 32, 0, GL_RGBA, GL_FLOAT, scattering_data);
	free(scattering_data);

	SDL_Surface *night_sky = load_physfs_image("/resources/textures/nightsky.png");
	glBindTexture(GL_TEXTURE_CUBE_MAP, stextures[STEXTURE_NIGHT_SKY]);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
	for (int i = 0; i < 6; i++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, night_sky->w, night_sky->h, 0, GL_RGBA,
			     GL_UNSIGNED_INT_8_8_8_8, night_sky->pixels);
	SDL_FreeSurface(night_sky);

	glBindTexture(GL_TEXTURE_3D, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static bool render_initialize_framebuffers(int width, int height)
{
	const int GLCATT0 = GL_COLOR_ATTACHMENT0;
	for (int pass = 0; pass < DEPTH_PEEL_PASSES; pass++) {
		glGenTextures(GBUF_LAST_TEXTURE, gbuffer + GBUF_FBIDX_MAX * pass);
		glGenRenderbuffers(GBUF_FIRST_FBUF - GBUF_LAST_TEXTURE, gbuffer + GBUF_FBIDX_MAX * pass + GBUF_LAST_TEXTURE);
		glGenFramebuffers(GBUF_FBIDX_MAX - GBUF_FIRST_FBUF, gbuffer + GBUF_FBIDX_MAX * pass + GBUF_FIRST_FBUF);

		glBindFramebuffer(GL_FRAMEBUFFER, GBUF(pass, GBUF_GEOMETRY_FBUF));

		/* Bind geometry buffer attachments */
		for (int j = GBUF_GEOMETRY_RBUFS; j < GBUF_LIGHTING_RBUFS; j++) {
			glBindTexture(GL_TEXTURE_2D, GBUF(pass, j));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			render_allocate_gbuffer_textures(width, height, j);
			glFramebufferTexture2D(GL_FRAMEBUFFER, j == GBUF_DEPTH_BUFFER ? GL_DEPTH_ATTACHMENT : GLCATT0 + j, GL_TEXTURE_2D,
					       GBUF(pass, j), 0);
		}
		glDrawBuffers(4, (const GLenum[]){ GLCATT0, GLCATT0 + 1, GLCATT0 + 2, GLCATT0 + 3 });

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			return false;

		glBindFramebuffer(GL_FRAMEBUFFER, GBUF(pass, GBUF_LIGHTING_FBUF));

		/* Bind lighting buffer color attachment textures (ambient/diffuse and specular) */
		for (int j = GBUF_LIGHTING_RBUFS; j < GBUF_LAST_TEXTURE; j++) {
			glBindTexture(GL_TEXTURE_2D, GBUF(pass, j));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			render_allocate_gbuffer_textures(width, height, j);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GLCATT0 + j - GBUF_LIGHTING_RBUFS, GL_TEXTURE_2D, GBUF(pass, j), 0);
		}
		glDrawBuffers(2, (const GLenum[]){ GLCATT0, GLCATT0 + 1 });

		/* Bind lighting buffer depth renderbuffer */
		glBindRenderbuffer(GL_RENDERBUFFER, GBUF(pass, GBUF_LIGHTING_DEPTH_RBUF));
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
					  GBUF(pass, GBUF_LIGHTING_DEPTH_RBUF));

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			return false;
	}

	glGenFramebuffers(SUN_SHADOW_CASCADES, sun_shadow);
	glGenTextures(1, sun_shadow + SUN_SHADOW_CASCADES);
	glBindTexture(GL_TEXTURE_2D_ARRAY, sun_shadow[SUN_SHADOW_CASCADES]);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT32F, SUN_SHADOW_SIZE, SUN_SHADOW_SIZE, SUN_SHADOW_CASCADES);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, (GLfloat[]){ 1.0f, 1.0f, 1.0f, 1.0f });
	for (int i = 0; i < SUN_SHADOW_CASCADES; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, sun_shadow[i]);
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, sun_shadow[SUN_SHADOW_CASCADES], 0, i);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			return false;
	}

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

static void render_generate_lightvol_meshes(GLuint sphere_vbo, GLuint sphere_ibo)
{
	/* The sphere has radius 1 and should be drawn with GL_TRIANGLES. */

	const int sphere_sectors = 8, sphere_stacks = 8;
	const float sector_step = 2 * M_PI / sphere_sectors, stack_step = M_PI / sphere_stacks;
	int i = 0;

	float sphere_verts[(sphere_sectors + 1) * (sphere_stacks + 1) * 3];
	GLubyte sphere_index[(sphere_sectors * (sphere_stacks - 1)) * 6];
	assert((sphere_sectors + 1) * (sphere_stacks + 1) * 3 < 256);

	for (int a = 0; a <= sphere_stacks; a++) {
		float stack_angle = M_PI / 2 - a * stack_step;
		float xy = cosf(stack_angle), z = sinf(stack_angle);
		for (int b = 0; b <= sphere_sectors; b++) {
			float sector_angle = b * sector_step;
			sphere_verts[i++] = xy * cosf(sector_angle);
			sphere_verts[i++] = xy * sinf(sector_angle);
			sphere_verts[i++] = z;
		}
	}

	i = 0;
	for (int a = 0; a < sphere_stacks; a++) {
		int k1 = a * (sphere_sectors + 1), k2 = k1 + sphere_sectors + 1;
		for (int b = 0; b < sphere_sectors; ++b, ++k1, ++k2) {
			if (a != 0) {
				sphere_index[i++] = k1;
				sphere_index[i++] = k2;
				sphere_index[i++] = k1 + 1;
			}
			if (a != (sphere_stacks - 1)) {
				sphere_index[i++] = k1 + 1;
				sphere_index[i++] = k2;
				sphere_index[i++] = k2 + 1;
			}
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(sphere_verts), sphere_verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sphere_index), sphere_index, GL_STATIC_DRAW);
}

bool render_init(int width, int height)
{
	use_shader(0);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(VBO_MAX, vbo);
	render_initialize_static_textures();
	if (render_initialize_framebuffers(width, height) == false)
		return false;

	render_generate_lightvol_meshes(vbo[VBO_LIGHTVOL_SPHERE], vbo[IBO_LIGHTVOL_SPHERE]);

	shaders[SHADER_BLOCKS] = create_shader("/resources/shaders/blocks.v.glsl", "/resources/shaders/blocks.f.glsl");
	shaders[SHADER_BLOCKPICK] = create_shader("/resources/shaders/blocks.v.glsl", "/resources/shaders/blockpick.f.glsl");
	shaders[SHADER_LIGHTSTENCIL] = create_shader("/resources/shaders/lightvol.v.glsl", "/resources/shaders/null.f.glsl");
	shaders[SHADER_LIGHTVOLUME] = create_shader("/resources/shaders/lightvol.v.glsl", "/resources/shaders/lightvol.f.glsl");
	shaders[SHADER_SKYLIGHT] = create_shader("/resources/shaders/blit.v.glsl", "/resources/shaders/skylight.f.glsl");
	shaders[SHADER_COMBINE_GBUF] = create_shader("/resources/shaders/blit.v.glsl", "/resources/shaders/combinegbuf.f.glsl");
	shaders[SHADER_SKY] = create_shader("/resources/shaders/sky.v.glsl", "/resources/shaders/sky.f.glsl");
	shaders[SHADER_DEPTHRENDER] = create_shader("/resources/shaders/depthmap.v.glsl", "/resources/shaders/depthmap.f.glsl");
	for (int i = 0; i < SHADER_MAX; i++) {
		if (shaders[i] == 0)
			return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glBindTexture(GL_TEXTURE_3D, 0);
	return true;
}

void render_deinit(void)
{
	glBindVertexArray(0);
	use_shader(0);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(VBO_MAX, vbo);
	glDeleteTextures(STEXTURE_MAX, stextures);

	for (int pass = 0; pass < DEPTH_PEEL_PASSES; pass++) {
		glDeleteTextures(GBUF_LAST_TEXTURE, gbuffer + GBUF_FBIDX_MAX * pass);
		glDeleteRenderbuffers(GBUF_FIRST_FBUF - GBUF_LAST_TEXTURE, gbuffer + GBUF_FBIDX_MAX * pass + GBUF_LAST_TEXTURE);
		glDeleteFramebuffers(GBUF_FBIDX_MAX - GBUF_FIRST_FBUF, gbuffer + GBUF_FBIDX_MAX * pass + GBUF_FIRST_FBUF);
	}

	glDeleteTextures(1, sun_shadow + SUN_SHADOW_CASCADES);
	glDeleteFramebuffers(SUN_SHADOW_CASCADES, sun_shadow);

	for (int i = 0; i < SHADER_MAX; i++)
		glDeleteProgram(shaders[i]);
}
