#pragma once
#include <cglm/cglm.h>
#include <GL/gl3w.h>
#include <stddef.h>
#include "blox.h"

#define CHUNK_WIDTH 25
#define CHUNK_HEIGHT 400
#define CHUNK_AREA (CHUNK_WIDTH * CHUNK_WIDTH)
#define CHUNK_TOTAL_BLOCKS (CHUNK_AREA * CHUNK_HEIGHT)
#define GRAVITY_PER_SECOND -28.0

static inline int CHUNK_BLOCK_INDEX(int x, int y, int z)
{
	return x + y * CHUNK_WIDTH + z * CHUNK_AREA;
}
static inline int CHUNK_BLOCK_INDEX2(int xy, int z)
{
	return xy + z * CHUNK_AREA;
}
static inline void WORLD_CHUNK(int x, int *chunk, int *offset)
{
	int chunk_value = (x >= 0 ? x : x - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
	if (chunk)
		*chunk = chunk_value;
	if (offset)
		*offset = x - chunk_value * CHUNK_WIDTH;
}

enum { VBUF_BLOCKS, VBUF_TRANSLUCENT, VBUF_MAX };

typedef struct block_instance_s {
	uint16_t id;
	uint8_t state;
	int skylight : 4;
} block_instance_t;

typedef struct chunk_s {
	int loc[2];
	block_instance_t blocks[CHUNK_TOTAL_BLOCKS];
	GLuint vbuf[VBUF_MAX];
	size_t vbufsize[VBUF_MAX];

	int num_lights;
	mat4 *light_data;

	int gen_stage : 7;
	bool dirty : 1;
} chunk_t;

typedef struct model_element_s {
	float cube[6];
	float uv[6][4];
	int textures[6];
	uint8_t faces, cull_faces;
} model_element_t;

typedef struct model_info_s {
	uint8_t num_elements, cull_neighbors : 6;
	model_element_t elements[0];
} model_info_t;

typedef struct pointlight_s {
	float luminosity[4];
	uint8_t color[3];
} pointlight_t;

typedef struct blockstate_s {
	model_info_t *model;
	pointlight_t pointlight;
	uint8_t fluid : 4;
	bool pickable : 1, opaque : 1, translucent : 1, scatter_skylight : 1;
} blockstate_t;

typedef struct blockdef_s {
	char *name;
	blockstate_t *states;
	uint8_t num_states;
} blockdef_t;

extern blockdef_t *blockdefs;

/* generate.c */
int world_request_chunkgen(int x, int y);
uint64_t world_seed(void);
void world_set_seed(uint64_t seed);

/* render.c */
#define VERTEX_DATA_SIZE 8 /* x, y, z, face (normal), u, v, texture, is_light?-1:1 */
int render_one_block(int x, int y, int z, bool preserve_uv, GLuint vbo);
void chunk_render(chunk_t *chunk);

/* storage.c */
void chunks_add(chunk_t *chunk);
void chunks_clear(void);
chunk_t *chunks_get(int x, int y);
void chunks_remove(int x, int y);
void world_init(void);
block_instance_t *world_get_block(int x, int y, int z);
void world_set_block(int x, int y, int z, block_instance_t *inst);

#define FACE_NAME_INDEX(X)                                                                                                                \
	(X == NULL ? -1 :                                                                                                                 \
		     (strcmp((X), "up") == 0 ?                                                                                            \
			      0 :                                                                                                         \
			      (strcmp((X), "down") == 0 ?                                                                                 \
				       1 :                                                                                                \
				       (strcmp((X), "north") == 0 ?                                                                       \
						2 :                                                                                       \
						(strcmp((X), "south") == 0 ?                                                              \
							 3 :                                                                              \
							 (strcmp((X), "east") == 0 ? 4 : (strcmp((X), "west") == 0 ? 5 : -1)))))))

void world_load_resources(void);
void world_init_workerpool(void);
