#pragma once
#include "blox.h"
#include <GL/gl3w.h>
#include <SDL.h>
#include <cglm/cglm.h>
#include <stddef.h>
#ifndef strdup
#define strdup SDL_strdup
#endif

/* hashtable.c */
typedef struct ht_item_s {
    char *key;
    void *value;
} ht_item_t;

typedef struct ht_table_s {
    int buckets, count;
    struct ht_item_s **data;
    bool owns_values;
} ht_table_t;
typedef ht_table_t htable_t;

htable_t *ht_init(int initial_capacity, bool owns_values);
void ht_clear(htable_t *table);
void ht_insert(htable_t *table, const char *key, void *value);
void *ht_get(htable_t *table, const char *key);
void *ht_delete(htable_t *table, const char *key);
void ht_resize(htable_t *table, int new_capacity);
void ht_deinit(htable_t *table);

/* physfs.c */
void *read_physfs_file(const char *path, size_t *length);
SDL_Surface *load_physfs_image(const char *path);

/* queue.c */
struct queue_node_s;
typedef struct queue_s {
    struct queue_node_s *head, *tail;
} queue_t;

queue_t *queue_create(void);
void queue_insert(queue_t *queue, void *v);
void *queue_pull(queue_t *queue);
int queue_size(queue_t *queue);
static inline bool queue_is_empty(queue_t *queue) {
    return queue->head == NULL;
}

/* rbtree.c */
typedef int (*rbtree_cmp_f)(const void *const a, const void *const b, void *extradata);
typedef void (*rbtree_rel_f)(void *key, void *value);
typedef struct rbtnode_s {
    struct rbtnode_s *parent, *children[2];
    void *key, *value;
    char color;
} rbtnode_t;
typedef struct rbtree_s {
    rbtnode_t *root;
    void *extradata;
    rbtree_cmp_f compare;
    rbtree_rel_f release;
} rbtree_t;

rbtree_t *rbtree_create(rbtree_cmp_f compare, rbtree_rel_f release, void *extradata);
void rbtree_destroy(rbtree_t *tree);
bool rbtree_insert(rbtree_t *tree, void *const key, void *const value);
void *rbtree_get(rbtree_t *const tree, const void *const key);
void rbtree_remove(rbtree_t *tree, const void *key);
void rbtree_removeall(rbtree_t *tree);
rbtnode_t *rbtree_next(rbtree_t *tree, const rbtnode_t *current);

/* shader.c */
GLuint create_shader_stage(const char *shader_path, GLenum type);
GLuint create_shader(const char *vertex_shader_path,
                                         const char *fragment_shader_path);
GLuint create_gs_shader(const char *vertex_shader_path,
                                                const char *geom_shader_path,
                                                const char *fragment_shader_path, GLint input,
                                                GLint output, GLint vertices);
void use_shader(GLuint program);
GLint get_shader_attrib(GLuint program, const char *name);
GLint get_shader_uniform(GLuint program, const char *name);

static inline void orientation_from_angles(vec3 orientation, double pitch, double yaw) {
    orientation[0] = cos(pitch) * cos(yaw);
    orientation[1] = cos(pitch) * sin(yaw);
    orientation[2] = sin(pitch);
}

/* threadpool.c */
typedef enum tpool_ret {
    TPOOL_SUCCESS = 0,
    TPOOL_FAILURE,
    TPOOL_RETRY_LATER
} tpool_ret_t;
struct tpool_s;
typedef struct tpool_s tpool_t;
typedef tpool_ret_t (*tpool_worker_t)(void *);

tpool_t *tpool_create(size_t workers);
void tpool_destroy(tpool_t *pool);
int tpool_add_work(tpool_t *pool, tpool_worker_t worker, void *arg,
                                     bool arg_owned);
int tpool_busy_workers(tpool_t *pool);
void tpool_wait(tpool_t *pool);
#ifdef _WIN32
#include <windows.h>
static inline unsigned int tpool_num_cores() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}
#else /* assume posix */
#include <unistd.h>
static inline unsigned int tpool_num_cores() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}
#endif /* _WIN32 */
