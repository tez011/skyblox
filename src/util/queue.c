#include <assert.h>
#include "util.h"

struct queue_node_s {
	struct queue_node_s *next;
	void *payload;
};

queue_t *queue_create(void)
{
	queue_t *q = malloc(sizeof(queue_t));
	assert(q);
	memset(q, 0, sizeof(queue_t));
	return q;
}

void queue_insert(queue_t *queue, void *v)
{
	struct queue_node_s *n = calloc(1, sizeof(struct queue_node_s));
	n->payload = v;

	if (queue->tail == NULL)
		queue->head = queue->tail = n;
	else {
		queue->tail->next = n;
		queue->tail = n;
	}
}

void *queue_pull(queue_t *queue)
{
	if (queue->head == NULL)
		return NULL;

	struct queue_node_s *n = queue->head;
	queue->head = queue->head->next;
	if (queue->head == NULL)
		queue->tail = NULL;

	void *v = n->payload;
	free(n);
	return v;
}

int queue_size(queue_t *queue)
{
	int size = 0;
	struct queue_node_s *it = queue->head;
	while (it) {
		size++;
		it = it->next;
	}
	return size;
}
