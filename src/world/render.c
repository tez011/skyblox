#include <assert.h>
#include <cglm/cglm.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"
#define VERTEX_PER_FACE 6

/** Which UV coordinate does this vertex correspond to? */
static uint8_t uv_idx[] = { 0, 1, 2, 1, 2, 3, 2, 3, 0, 3, 0, 1 };

/** Cubes can be defined by two points in 3D space. For each coordinate of
 * the cube to render, which of those six base coordinates does it come from? */
static uint8_t fv_idx[6][18] = {
	{ 0, 1, 5, 3, 1, 5, 3, 4, 5, 3, 4, 5, 0, 4, 5, 0, 1, 5 }, /* up */
	{ 0, 1, 2, 0, 4, 2, 3, 4, 2, 3, 4, 2, 3, 1, 2, 0, 1, 2 }, /* down */
	{ 0, 4, 5, 3, 4, 5, 3, 4, 2, 3, 4, 2, 0, 4, 2, 0, 4, 5 }, /* north */
	{ 3, 1, 5, 0, 1, 5, 0, 1, 2, 0, 1, 2, 3, 1, 2, 3, 1, 5 }, /* south */
	{ 3, 4, 5, 3, 1, 5, 3, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5 }, /* east */
	{ 0, 1, 5, 0, 4, 5, 0, 4, 2, 0, 4, 2, 0, 1, 2, 0, 1, 5 }, /* west */
};

static inline blockstate_t *get_block_state(block_instance_t *blk)
{
	if (blk != NULL)
		return &blockdefs[blk->id].states[blk->state];
	else
		return NULL;
}

int render_one_block(int x, int y, int z, bool preserve_uv, GLuint vbo)
{
	blockstate_t *bstate = get_block_state(world_get_block(x, y, z));
	model_info_t *mdl = bstate ? bstate->model : NULL;
	if (bstate == NULL || mdl == NULL || mdl->num_elements == 0) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
		return 0;
	}

	float buffer[mdl->num_elements * 6 * 6 * VERTEX_DATA_SIZE];
	int num_verts = 0;
	for (model_element_t *el = mdl->elements; el < mdl->elements + mdl->num_elements; el++) {
		for (int facevert = 0; facevert < 6 * 6; facevert++) {
			int fi = facevert / 6, vert = facevert % 6;
			for (int i = 0; i < 3; i++)
				buffer[(num_verts * VERTEX_DATA_SIZE) + 0 + i] = el->cube[fv_idx[fi][vert * 3 + i]];
			buffer[(num_verts * VERTEX_DATA_SIZE) + 4] = fi;
			for (int i = 0; i < 2; i++) {
				float uvv = preserve_uv ? el->uv[fi][uv_idx[vert * 2 + i]] : uv_idx[vert * 2 + i] >= 2;
				buffer[(num_verts * VERTEX_DATA_SIZE) + 4 + i] = uvv;
			}
			buffer[(num_verts * VERTEX_DATA_SIZE) + 6] = preserve_uv ? el->textures[fi] : 0;
			num_verts++;
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, num_verts * VERTEX_DATA_SIZE * sizeof(float), buffer, GL_DYNAMIC_DRAW);
	return num_verts;
}

void chunk_render(chunk_t *chunk)
{
	if (chunk == NULL || chunk->dirty == false)
		return;

	size_t num_vertices[VBUF_MAX] = { 0 }, max_vertices[VBUF_MAX];
	float *vtx[VBUF_MAX];
	int num_lights = 0;
	for (int vb = 0; vb < VBUF_MAX; vb++) {
		max_vertices[vb] = CHUNK_AREA * 3 * 2 * 6 * 2;
		vtx[vb] = malloc(max_vertices[vb] * VERTEX_DATA_SIZE * sizeof(float));
		assert(vtx[vb]);
	}

	/* Render the blocks to a VBO, and count the number of lights. */
	for (int bi = 0; bi < CHUNK_TOTAL_BLOCKS; bi++) {
		block_instance_t *binst = chunk->blocks + bi;
		assert(binst->state < blockdefs[binst->id].num_states);

		int bx = bi % CHUNK_WIDTH, by = (bi / CHUNK_WIDTH) % CHUNK_WIDTH, bz = bi / CHUNK_AREA;
		blockstate_t *bstate = &blockdefs[binst->id].states[binst->state];
		model_info_t *model = bstate->model;
		if (model == NULL)
			continue;

		if (bstate->pointlight.luminosity[0] != 0)
			num_lights++;

		for (model_element_t *el = model->elements; el < model->elements + model->num_elements; el++) {
			for (int fi = 0; fi < 6; fi++) {
				bool draw_face = (el->faces & (1 << fi)) != 0, cull_face = false;
				if ((el->cull_faces & (1 << fi)) != 0) {
					/* Check the neighbor to see if it's possible to cull */
					block_instance_t *nbinst = world_get_block(chunk->loc[0] * CHUNK_WIDTH + bx + cube_normal[fi][0],
										   chunk->loc[1] * CHUNK_WIDTH + by + cube_normal[fi][1],
										   bz + cube_normal[fi][2]);
					blockstate_t *nbst = get_block_state(nbinst);
					if (nbst == NULL || nbst->model == NULL)
						cull_face = false;
					else if (nbst->opaque || nbinst->id == binst->id)
						cull_face = (nbst->model->cull_neighbors & (1 << fi)) != 0;
					else if (nbst->translucent && bstate->translucent)
						cull_face = (nbst->model->cull_neighbors & (1 << fi)) != 0 && nbinst->id > binst->id;
				}

				int dest_vbuf = -1;
				if (draw_face && !cull_face) {
					if (bstate->opaque)
						dest_vbuf = VBUF_BLOCKS;
					else if (bstate->translucent)
						dest_vbuf = VBUF_TRANSLUCENT;
					else
						continue;
				} else
					continue;

				float face_data[VERTEX_PER_FACE * VERTEX_DATA_SIZE];
				for (int vert = 0; vert < 6; vert++) {
					face_data[vert * VERTEX_DATA_SIZE + 0] = bx + el->cube[fv_idx[fi][vert * 3 + 0]];
					face_data[vert * VERTEX_DATA_SIZE + 1] = by + el->cube[fv_idx[fi][vert * 3 + 1]];
					face_data[vert * VERTEX_DATA_SIZE + 2] = bz + el->cube[fv_idx[fi][vert * 3 + 2]];
					face_data[vert * VERTEX_DATA_SIZE + 3] =
						(bstate->pointlight.luminosity[0] == 0 ? 1 : -1) * (fi + 1);
					if (dest_vbuf == VBUF_BLOCKS || dest_vbuf == VBUF_TRANSLUCENT) {
						face_data[vert * VERTEX_DATA_SIZE + 4] = el->uv[fi][uv_idx[vert * 2 + 0]];
						face_data[vert * VERTEX_DATA_SIZE + 5] = el->uv[fi][uv_idx[vert * 2 + 1]];
						face_data[vert * VERTEX_DATA_SIZE + 6] = el->textures[fi];
						face_data[vert * VERTEX_DATA_SIZE + 7] = bstate->pointlight.luminosity[0] == 0 ? 1 : -1;
					}
				}

				if (max_vertices[dest_vbuf] < num_vertices[dest_vbuf] + VERTEX_PER_FACE) {
					float *nvtx = realloc(vtx[dest_vbuf], sizeof(float) * max_vertices[dest_vbuf] * 4 / 3);
					assert(nvtx);
					vtx[dest_vbuf] = nvtx;
					max_vertices[dest_vbuf] = max_vertices[dest_vbuf] * 4 / 3;
				}
				memcpy(&vtx[dest_vbuf][num_vertices[dest_vbuf] * VERTEX_DATA_SIZE], face_data,
				       VERTEX_PER_FACE * VERTEX_DATA_SIZE * sizeof(float));
				num_vertices[dest_vbuf] += VERTEX_PER_FACE;
			}
		}
	}

	/* Gather information on the point lights in the chunk. */
	if (chunk->num_lights != num_lights || chunk->light_data == NULL) {
		mat4 *nld = realloc(chunk->light_data, num_lights * sizeof(mat4));
		chunk->num_lights = num_lights;
		chunk->light_data = nld;
		for (int bi = 0, li = 0; bi < CHUNK_TOTAL_BLOCKS; bi++) {
			block_instance_t *binst = chunk->blocks + bi;
			blockstate_t *bstate = &blockdefs[binst->id].states[binst->state];
			if (bstate->pointlight.luminosity[0] == 0)
				continue;

			chunk->light_data[li][0][0] = bi % CHUNK_WIDTH;
			chunk->light_data[li][0][1] = (bi / CHUNK_WIDTH) % CHUNK_WIDTH;
			chunk->light_data[li][0][2] = bi / CHUNK_AREA;
			chunk->light_data[li][0][3] = bstate->pointlight.luminosity[3];
			for (int j = 0; j < 3; j++) {
				chunk->light_data[li][1][j] = bstate->pointlight.color[j] / 255.f;
				chunk->light_data[li][2][j] = bstate->pointlight.luminosity[j];
			}
			li++;
		}
	}

	if (chunk->vbuf[0] == 0)
		glGenBuffers(VBUF_MAX, chunk->vbuf);

	for (int vb = 0; vb < VBUF_MAX; vb++) {
		glBindBuffer(GL_ARRAY_BUFFER, chunk->vbuf[vb]);
		glBufferData(GL_ARRAY_BUFFER, num_vertices[vb] * VERTEX_DATA_SIZE * sizeof(float), vtx[vb], GL_DYNAMIC_DRAW);
		chunk->vbufsize[vb] = num_vertices[vb];
		free(vtx[vb]);
	}

	chunk->dirty = false;
}
