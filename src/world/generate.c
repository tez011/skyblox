#include "util.h"
#include "world.h"

static uint64_t chunk_gen_seed = 0;
static tpool_t *world_threadpool = NULL;

static void generate_chunk_blocks(chunk_t *chunk, uint64_t seed)
{
	memset(chunk->blocks, 0, sizeof(block_instance_t) * CHUNK_TOTAL_BLOCKS);
	for (int i = 0; i < CHUNK_AREA; i++)
		chunk->blocks[i].id = 1;
	for (int i = 0; i < CHUNK_AREA * 2; i++)
		chunk->blocks[CHUNK_AREA + i].id = 2;
	for (int i = 0; i < CHUNK_AREA; i++)
		chunk->blocks[CHUNK_AREA * 3 + i].id = 3;
}

/****************************************************************************/

static inline void world_deinit_workerpool(void)
{
	tpool_destroy(world_threadpool);
}

static tpool_ret_t chunk_generate_worker(void *_chunk)
{
	chunk_t *chunk = _chunk;
	bool change_made = true;
	if (chunk->gen_stage == 0) {
		generate_chunk_blocks(chunk, chunk_gen_seed);
	}

	if (change_made) {
		chunk->gen_stage++;
		chunk->dirty = true;
		for (int f = FACE_NORTH; f < FACE_MAX; f++) {
			chunk_t *n = chunks_get(chunk->loc[0] + cube_normal[f][0], chunk->loc[1] + cube_normal[f][1]);
			if (n)
				n->dirty = true;
		}
	}

	return TPOOL_SUCCESS;
}

void world_init_workerpool(void)
{
	world_threadpool = tpool_create(tpool_num_cores() * 2);
	atexit(world_deinit_workerpool);
}

uint64_t world_seed(void)
{
	return chunk_gen_seed;
}

void world_set_seed(uint64_t seed)
{
	chunk_gen_seed = seed;
}

int world_request_chunkgen(int x, int y)
{
	if (chunks_get(x, y) != NULL)
		return -1; /* the chunk already exists, or is being created */

	chunk_t *chunk = calloc(1, sizeof(chunk_t));
	chunk->loc[0] = x;
	chunk->loc[1] = y;
	chunks_add(chunk);

	tpool_add_work(world_threadpool, chunk_generate_worker, chunk, false);

	return 0;
}
