#include "util.h"
#include "world.h"

int8_t cube_normal[FACE_MAX][3] = { { 0, 0, 1 }, { 0, 0, -1 }, { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
static rbtree_t *chunktree = NULL;

static int chunktree_cmp(const void *const a, const void *const b, void *extradata)
{
	/* If a and b point to chunks, they also point to the first element in the chunk structure,
	 * which happens to be an int[2]. This allows comparisons between chunks and int[2]'s, a
	 * necessary operation when trying to find a chunk. */
	const int *const c1 = a, *const c2 = b;
	UNUSED(extradata);
	if (c1[0] == c2[0])
		return c2[1] - c1[1];
	return c2[0] - c1[0];
}

static void chunktree_rel(void *key, void *value)
{
	if (key != value) {
		fprintf(stderr, "FATAL: Chunk tree invariant (key != value) was violated.\n");
		abort();
	}
	free(key);
}

void chunks_deinit(void)
{
	rbtree_destroy(chunktree);
}

void chunks_add(chunk_t *chunk)
{
	rbtree_insert(chunktree, chunk, chunk);
}

void chunks_clear(void)
{
	rbtree_removeall(chunktree);
}

chunk_t *chunks_get(int x, int y)
{
	int loc[] = { x, y };
	return rbtree_get(chunktree, loc);
}

void chunks_remove(int x, int y)
{
	int loc[] = { x, y };
	rbtree_remove(chunktree, loc);
}

void world_init(void)
{
	world_load_resources();
	world_init_workerpool();

	chunktree = rbtree_create(chunktree_cmp, chunktree_rel, NULL);
}

block_instance_t *world_get_block(int x, int y, int z)
{
	int chunkloc[2], xoff, yoff;
	WORLD_CHUNK(x, &chunkloc[0], &xoff);
	WORLD_CHUNK(y, &chunkloc[1], &yoff);

	chunk_t *chunk = rbtree_get(chunktree, chunkloc);
	if (chunk != NULL && z >= 0 && z < CHUNK_HEIGHT)
		return chunk->blocks + CHUNK_BLOCK_INDEX(xoff, yoff, z);
	else
		return NULL;
}

void world_set_block(int x, int y, int z, block_instance_t *inst)
{
	int chunkloc[2], xoff, yoff;
	WORLD_CHUNK(x, &chunkloc[0], &xoff);
	WORLD_CHUNK(y, &chunkloc[1], &yoff);

	chunk_t *chunk = rbtree_get(chunktree, chunkloc);
	if (chunk != NULL) {
		memcpy(chunk->blocks + CHUNK_BLOCK_INDEX(xoff, yoff, z), inst, sizeof(block_instance_t));
		/* some callbacks will be necessary here */

		chunk->dirty = true;

		if (xoff == 0) {
			chunk_t *nb = chunks_get(chunkloc[0] - 1, chunkloc[1]);
			if (nb)
				nb->dirty = true;
		} else if (xoff == CHUNK_WIDTH - 1) {
			chunk_t *nb = chunks_get(chunkloc[0] + 1, chunkloc[1]);
			if (nb)
				nb->dirty = true;
		}
		if (yoff == 0) {
			chunk_t *nb = chunks_get(chunkloc[0], chunkloc[1] - 1);
			if (nb)
				nb->dirty = true;
		} else if (yoff == CHUNK_WIDTH - 1) {
			chunk_t *nb = chunks_get(chunkloc[0], chunkloc[1] + 1);
			if (nb)
				nb->dirty = true;
		}
	}
}
