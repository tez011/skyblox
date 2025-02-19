#pragma once
#include <assert.h>
#include "cglm/cglm.h"
#include "ingame.h"
#include "render.h"
#define RENDER_NEAR 0.1f
#define RENDER_FAR 500.f
#define DEPTH_PEEL_PASSES 1
#define SUN_SHADOW_CASCADES 4
#define SUN_SHADOW_SIZE 1024

#define GBUF(PASS, IDX) (gbuffer[GBUF_FBIDX_MAX * (PASS) + (IDX)])
enum gbuffer_it {
	GBUF_POSITION,
	GBUF_NORMAL,
	GBUF_ALBEDO_TRANSPARENCY,
	GBUF_SPECULAR,
	GBUF_DEPTH_BUFFER,
	GBUF_AMBDIF_LIGHTING,
	GBUF_SPEC_LIGHTING,
	GBUF_LIGHTING_DEPTH_RBUF,
	GBUF_GEOMETRY_FBUF,
	GBUF_LIGHTING_FBUF,
	GBUF_FBIDX_MAX,

	GBUF_FIRST_FBUF = GBUF_GEOMETRY_FBUF,
	GBUF_LAST_TEXTURE = GBUF_LIGHTING_DEPTH_RBUF,
	GBUF_GEOMETRY_RBUFS = GBUF_POSITION,
	GBUF_LIGHTING_RBUFS = GBUF_AMBDIF_LIGHTING,
};

enum { SHADER_BLOCKS,
       SHADER_BLOCKPICK,
       SHADER_LIGHTSTENCIL,
       SHADER_LIGHTVOLUME,
       SHADER_SKYLIGHT,
       SHADER_COMBINE_GBUF,
       SHADER_SKY,
       SHADER_DEPTHRENDER,
       SHADER_MAX };

enum { STEXTURE_SKY_SCATTERING, STEXTURE_NIGHT_SKY, STEXTURE_MAX };

enum { VBO_BLOCKPICK, VBO_LIGHTVOL_SPHERE, IBO_LIGHTVOL_SPHERE, VBO_LIGHTPROPS, VBO_MAX };

extern int g_screen_width, g_screen_height;

extern GLuint block_textures;
extern GLuint vao, vbo[VBO_MAX], shaders[SHADER_MAX], stextures[STEXTURE_MAX];
extern GLuint gbuffer[DEPTH_PEEL_PASSES * GBUF_FBIDX_MAX];
extern GLuint sun_shadow[SUN_SHADOW_CASCADES + 1];

float sphere_verts[10 * 8 * 3];
GLubyte sphere_index[10 * 8 * 6];

/* main, referenced elsewhere */
void render_chunk_buffers(int cx, int cy, int vb, vec4 *vf_planes);

/* init */
bool render_init(int initial_width, int initial_height);
void render_deinit(void);
void render_allocate_gbuffer_textures(int width, int height, int buffer_slice);

void render_main(SDL_Window *window, struct nk_context *ui_ctx);
void render_viewport_change(int width, int height);

/* light */
int buffer_light_data(int cx, int cy);
void draw_pointlights(mat4 vp, int num_lights);
void draw_skylights(mat4 *lightspace, float *proj_cascade_planes);
void draw_skyshadow_maps(int cx, int cy, vec4 *vf_corners, mat4 *lightspace, float *proj_cascade_planes);