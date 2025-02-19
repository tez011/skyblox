#include <time.h>
#include "tinycthread.h"
#include "util.h"

typedef struct tpool_work_s {
	tpool_worker_t fn;
	void *arg;
	bool arg_owned;
} tpool_work_t;

struct tpool_s {
	queue_t work_queue;
	mtx_t work_mutex;
	cnd_t work_cond, working_cond;
	size_t num_busy, total_threads;
	bool stop;
};

static inline void tpool_insert_work(tpool_t *pool, tpool_work_t *work)
{
	mtx_lock(&pool->work_mutex);
	queue_insert(&pool->work_queue, work);
	cnd_broadcast(&pool->work_cond);
	mtx_unlock(&pool->work_mutex);
}

static int tpool_worker(void *arg)
{
	tpool_t *pool = arg;
	tpool_work_t *work;

	while (true) {
		mtx_lock(&pool->work_mutex);
		if (pool->stop)
			break;

		if (queue_is_empty(&pool->work_queue))
			cnd_wait(&pool->work_cond, &pool->work_mutex);

		work = queue_pull(&pool->work_queue);
		pool->num_busy++;
		mtx_unlock(&pool->work_mutex);

		if (work) {
			tpool_ret_t r = work->fn(work->arg);
			if (r == TPOOL_RETRY_LATER) {
				tpool_insert_work(pool, work);
			} else {
				if (work->arg_owned && work->arg)
					free(work->arg);
				free(work);
			}
			SDL_Delay(100);
		}

		mtx_lock(&pool->work_mutex);
		pool->num_busy--;
		if (!pool->stop && pool->num_busy == 0 && queue_is_empty(&pool->work_queue))
			cnd_signal(&pool->working_cond);
		mtx_unlock(&pool->work_mutex);
	}

	pool->total_threads--;
	cnd_signal(&pool->working_cond);
	mtx_unlock(&pool->work_mutex);
	return 0;
}

tpool_t *tpool_create(size_t workers)
{
	thrd_t thread;
	tpool_t *pool = calloc(1, sizeof(tpool_t));
	workers = MIN(workers, 2);
	pool->total_threads = workers;
	mtx_init(&pool->work_mutex, mtx_plain);
	cnd_init(&pool->work_cond);
	cnd_init(&pool->working_cond);

	for (size_t i = 0; i < workers; i++) {
		thrd_create(&thread, tpool_worker, pool);
		thrd_detach(thread);
	}
	return pool;
}

void tpool_destroy(tpool_t *pool)
{
	tpool_work_t *work;
	if (pool == NULL)
		return;

	mtx_lock(&pool->work_mutex);
	while (!queue_is_empty(&pool->work_queue)) {
		work = queue_pull(&pool->work_queue);
		if (work->arg_owned && work->arg)
			free(work->arg);
		free(work);
	}
	pool->stop = true;
	cnd_broadcast(&pool->work_cond);
	mtx_unlock(&pool->work_mutex);

	mtx_destroy(&pool->work_mutex);
	cnd_destroy(&pool->work_cond);
	cnd_destroy(&pool->working_cond);
	free(pool);
}

int tpool_add_work(tpool_t *pool, tpool_worker_t worker, void *arg, bool arg_owned)
{
	tpool_work_t *work;
	if (pool == NULL)
		return -1;

	work = malloc(sizeof(tpool_work_t));
	work->fn = worker;
	work->arg = arg;
	work->arg_owned = arg_owned;

	tpool_insert_work(pool, work);
	return 0;
}

int tpool_busy_workers(tpool_t *pool)
{
	return pool->num_busy;
}

void tpool_wait(tpool_t *pool)
{
	if (pool) {
		mtx_lock(&pool->work_mutex);
		while (true) {
			if ((pool->stop == false && pool->num_busy != 0) || (pool->stop && pool->total_threads != 0))
				cnd_wait(&pool->working_cond, &pool->work_mutex);
			else
				break;
		}
		mtx_unlock(&pool->work_mutex);
	}
}
