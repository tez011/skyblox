#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#define COLOR_RED 'R'
#define COLOR_BLACK 'B'
#define NODE_IS_LEFT_CHILD(X) ((X)->parent && (X)->parent->children[0] == (X))
#define NODE_IS_RIGHT_CHILD(X) ((X)->parent && (X)->parent->children[1] == (X))
#define NODE_IS_BLACK(X) ((X) == NULL || (X)->color == COLOR_BLACK)
#define NODE_IS_RED(X) ((X) && (X)->color == COLOR_RED)
#define LEFT_ROTATION 0
#define RIGHT_ROTATION 1
#define RBTCMP(tree, a, b) ((tree)->compare((a), (b), (tree)->extradata))

static rbtnode_t *rbtsibling(rbtnode_t *n)
{
	if (n->parent) {
		for (int i = 0; i < 2; i++) {
			if (n->parent->children[i] == n)
				return n->parent->children[1 - i];
		}
	}
	return NULL;
}

static inline rbtnode_t *rbtuncle(rbtnode_t *n)
{
	if (n->parent)
		return rbtsibling(n->parent);
	else
		return NULL;
}

static inline void rbtrotate(rbtree_t *tree, rbtnode_t *self, int dir)
{
	rbtnode_t *res = self->children[1 - dir];
	self->children[1 - dir] = res->children[dir];
	if (self->parent == NULL) {
		tree->root = res;
	} else {
		self->parent->children[NODE_IS_RIGHT_CHILD(self)] = res;
	}
	res->children[dir] = self;

	res->parent = self->parent;
	self->parent = res;
}

static void rbtinsfixup(rbtree_t *tree, rbtnode_t *z)
{
	z->color = COLOR_RED;
	if (z && z->parent && NODE_IS_RED(z->parent)) {
		if (NODE_IS_RED(rbtuncle(z))) {
			z->parent->color = COLOR_BLACK;
			rbtuncle(z)->color = COLOR_BLACK;
			rbtinsfixup(tree, z->parent->parent);
		} else if (NODE_IS_LEFT_CHILD(z->parent)) {
			if (NODE_IS_RIGHT_CHILD(z)) {
				rbtrotate(tree, z = z->parent, LEFT_ROTATION);
			}
			z->parent->color = COLOR_BLACK;
			z->parent->parent->color = COLOR_RED;
			rbtrotate(tree, z->parent->parent, RIGHT_ROTATION);
		} else if (NODE_IS_RIGHT_CHILD(z->parent)) {
			if (NODE_IS_LEFT_CHILD(z)) {
				rbtrotate(tree, z = z->parent, RIGHT_ROTATION);
			}
			z->parent->color = COLOR_BLACK;
			z->parent->parent->color = COLOR_RED;
			rbtrotate(tree, z->parent->parent, LEFT_ROTATION);
		}
	}
	tree->root->color = COLOR_BLACK;
}

static void rbtdelfixup(rbtree_t *tree, rbtnode_t *n)
{
	while (n && n->parent && NODE_IS_BLACK(n)) {
		int dir = NODE_IS_RIGHT_CHILD(n);
		rbtnode_t *sib = n->parent->children[1 - dir];
		if (NODE_IS_RED(sib)) {
			sib->color = COLOR_BLACK;
			n->parent->color = COLOR_RED;
			rbtrotate(tree, n->parent, dir);
			sib = n->parent->children[1 - dir];
		}
		if (NODE_IS_BLACK(sib->children[0]) && NODE_IS_BLACK(sib->children[1])) {
			sib->color = COLOR_RED;
			n = n->parent;
		} else {
			if (NODE_IS_BLACK(sib->children[1 - dir])) {
				sib->children[dir]->color = COLOR_BLACK;
				sib->color = COLOR_RED;
				rbtrotate(tree, sib, 1 - dir);
				sib = n->parent->children[1 - dir];
			}
			sib->color = n->parent->color;
			n->parent->color = COLOR_BLACK;
			sib->children[1 - dir]->color = COLOR_BLACK;
			rbtrotate(tree, n->parent, dir);
			break;
		}
	}
	n->color = COLOR_BLACK;
}

static void rbtdelnode(rbtree_t *tree, rbtnode_t *n)
{
	for (int i = 0; i < 2; i++) {
		if (n->children[i])
			rbtdelnode(tree, n->children[i]);
	}
	if (tree->release)
		tree->release(n->key, n->value);
	free(n);
}

static inline rbtnode_t *create_node_for(void *const key, void *const value)
{
	rbtnode_t *n = calloc(1, sizeof(rbtnode_t));
	n->color = COLOR_BLACK;
	n->key = key;
	n->value = value;
	return n;
}

rbtree_t *rbtree_create(rbtree_cmp_f compare, rbtree_rel_f release, void *extradata)
{
	rbtree_t *t = calloc(1, sizeof(rbtree_t));
	t->extradata = extradata;
	t->compare = compare;
	t->release = release;
	return t;
}

/* precondition: extradata has been dealt with before this is called */
void rbtree_destroy(rbtree_t *tree)
{
	rbtree_removeall(tree);
	free(tree);
}

bool rbtree_insert(rbtree_t *tree, void *const key, void *const value)
{
	if (tree->root == NULL) {
		tree->root = create_node_for(key, value);
		return true;
	}

	rbtnode_t *it = tree->root, *p = NULL;
	while (it) {
		int cmp = RBTCMP(tree, key, it->key);
		p = it;
		if (cmp == 0)
			return false;
		else
			it = it->children[cmp > 0];
	}

	rbtnode_t *n = create_node_for(key, value);
	p->children[RBTCMP(tree, key, p->key) > 0] = n;
	n->parent = p;
	rbtinsfixup(tree, n);
	return true;
}

static inline rbtnode_t *rbtgetnode(rbtree_t *const tree, const void *const key)
{
	rbtnode_t *it = tree->root;
	while (it) {
		int cmp = RBTCMP(tree, key, it->key);
		if (cmp == 0)
			break;
		else
			it = it->children[cmp > 0];
	}
	return it;
}

void *rbtree_get(rbtree_t *const tree, const void *const key)
{
	rbtnode_t *n = rbtgetnode(tree, key);
	if (n == NULL)
		return NULL;
	else
		return n->value;
}

void rbtree_remove(rbtree_t *tree, const void *key)
{
	rbtnode_t *x = rbtgetnode(tree, key);
	if (x == NULL)
		return;

	/* If the node we want to remove has two children, copy its predecessor into it,
	 * so that the node we want to remove becomes the predecessor with less than two
	 * children.  */
	if (x->children[0] && x->children[1]) {
		rbtnode_t *predecessor = x->children[0];
		while (predecessor && predecessor->children[1])
			predecessor = predecessor->children[1];

		if (tree->release)
			tree->release(x->key, x->value);

		x->key = predecessor->key;
		x->value = predecessor->value;
		x = predecessor;
	}

	rbtnode_t *pullup = x->children[0] ? x->children[0] : x->children[1];
	if (pullup) { /* Has a child. */
		if (x->parent == NULL) {
			tree->root = pullup;
			pullup->parent = NULL;
		} else {
			x->parent->children[NODE_IS_RIGHT_CHILD(x)] = pullup;
			pullup->parent = x->parent;
		}
		if (NODE_IS_BLACK(x))
			rbtdelfixup(tree, pullup);
	} else if (x->parent == NULL) { /* Is the only element in the tree. */
		tree->root = NULL;
	} else { /* Is leaf. */
		if (NODE_IS_BLACK(x))
			rbtdelfixup(tree, x);
		x->parent->children[NODE_IS_RIGHT_CHILD(x)] = NULL;
	}

	/* Discard the node. */
	if (tree->release)
		tree->release(x->key, x->value);
	free(x);
}

void rbtree_removeall(rbtree_t *tree)
{
	if (tree->root)
		rbtdelnode(tree, tree->root);
}

rbtnode_t *rbtree_next(rbtree_t *tree, const rbtnode_t *current)
{
	rbtnode_t *successor;
	if (current->children[1]) {
		successor = current->children[1];
		while (successor->children[0])
			successor = successor->children[0];
	} else {
		successor = current->parent;
		while (successor && RBTCMP(tree, successor->key, current->key) <= 0)
			successor = successor->parent;
	}
	return successor;
}
