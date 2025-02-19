#include <math.h>
#include <string.h>
#include "util.h"
#define strdup SDL_strdup

static const int HT_MIN_SIZE = 17;

static int ht_base_hash(const char *s, const int a, const int m)
{
	/* "a" should be a prime number larger than the size of the alphabet (256 for an 8-bit value). */
	long hash = 0;
	const int len_s = strlen(s);
	for (int i = 0; i < len_s; i++) {
		hash += pow(a, len_s - i - 1) * s[i];
		hash = hash % m;
	}
	return hash;
}

static int ht_hash(const char *s, const int num_buckets, const int attempt)
{
	const int hash_a = ht_base_hash(s, 269, num_buckets), hash_b = ht_base_hash(s, 317, num_buckets - 1);
	return (hash_a + attempt * (hash_b + 1)) % num_buckets;
}

static bool is_maybe_prime(const int x)
{
	if (x < 2)
		return 0;
	if (x < 4)
		return 1;
	if ((x % 2) == 0)
		return 0;
	for (int i = 3; i <= MIN(101, floor(sqrt(x))); i += 2) {
		if ((x % i) == 0)
			return 0;
	}
	return 1;
}

static int next_maybe_prime(int x)
{
	if (x < 2)
		return 2;
	if (x == 2)
		return 3;
	x -= 1 - (x % 2);
	do {
		x += 2;
	} while (is_maybe_prime(x) == 0);
	return x;
}

ht_table_t *ht_init(int initial_capacity, bool owns_values)
{
	ht_table_t *table = malloc(sizeof(ht_table_t));
	table->buckets = initial_capacity < HT_MIN_SIZE ? HT_MIN_SIZE : next_maybe_prime(initial_capacity);
	table->count = 0;
	table->owns_values = owns_values;
	table->data = calloc(table->buckets, sizeof(ht_item_t));
	return table;
}

void ht_deinit(ht_table_t *table)
{
	ht_clear(table);
	free(table->data);
	free(table);
}

static ht_item_t HT_DELETED_ITEM = { NULL, NULL };
void ht_clear(ht_table_t *table)
{
	for (int i = 0; i < table->buckets; i++) {
		ht_item_t *td = table->data[i];
		if (td != NULL && td != &HT_DELETED_ITEM) {
			free(td->key);
			if (table->owns_values)
				free(td->value);
			free(td);
			td = NULL;
			table->count--;
		}
	}
}

void ht_insert(ht_table_t *table, const char *key, void *value)
{
	int index, i = 0;
	ht_item_t *cur;

	const int load = table->count * 100 / table->buckets;
	if (load > 70)
		ht_resize(table, table->buckets * 5 / 3);

	do {
		index = ht_hash(key, table->buckets, i++);
		cur = table->data[index];
		if (cur && strcmp(cur->key, key) == 0) {
			if (table->owns_values)
				free(cur->value);
			cur->value = value;
			return;
		}
	} while (cur != NULL && cur != &HT_DELETED_ITEM);

	table->data[index] = malloc(sizeof(ht_item_t));
	table->data[index]->key = strdup(key);
	table->data[index]->value = value;
	table->count++;
}

void *ht_get(ht_table_t *table, const char *key)
{
	int index, i = 0;
	ht_item_t *cur = NULL;
	do {
		if (cur && cur != &HT_DELETED_ITEM && strcmp(cur->key, key) == 0)
			return cur->value;

		index = ht_hash(key, table->buckets, i++);
		cur = table->data[index];
	} while (cur != NULL);

	return NULL;
}

void *ht_delete(ht_table_t *table, const char *key)
{
	int index = ht_hash(key, table->buckets, 0), i = 1;
	ht_item_t *cur = table->data[index];
	void *deleted_value = NULL;
	while (cur) {
		if (cur != &HT_DELETED_ITEM && strcmp(cur->key, key) == 0) {
			free(cur->key);
			if (table->owns_values)
				free(cur->value);
			else
				deleted_value = cur->value;
			free(cur);
			table->data[index] = &HT_DELETED_ITEM;
			table->count--;
			break;
		}
		index = ht_hash(key, table->buckets, i++);
		cur = table->data[index];
	}

	const int load = table->count * 100 / table->buckets;
	if (load < 10)
		ht_resize(table, table->buckets * 3 / 5);

	return deleted_value;
}

void ht_resize(ht_table_t *table, int new_capacity)
{
	int old_cap = table->buckets, n_cap = new_capacity < HT_MIN_SIZE ? HT_MIN_SIZE : next_maybe_prime(new_capacity);
	ht_item_t **all_items = table->data;
	if (n_cap < table->count)
		return; /* Can't resize and maintain all items */

	table->buckets = n_cap;
	table->data = calloc(n_cap, sizeof(ht_item_t));
	for (int i = 0; i < old_cap; i++) {
		if (all_items[i] == NULL || all_items[i] == &HT_DELETED_ITEM)
			continue;

		int attempt = 0, index;
		ht_item_t *cur;
		do {
			index = ht_hash(all_items[i]->key, n_cap, attempt++);
			cur = table->data[index];
		} while (cur != NULL);

		table->data[index] = malloc(sizeof(ht_item_t));
		table->data[index]->key = all_items[i]->key;
		table->data[index]->value = all_items[i]->value;
		free(all_items[i]);
	}
	free(all_items);
}

#if 0
#include <assert.h>
void ht_test()
{
	ht_table_t *table1 = ht_init(0, false), *table2 = ht_init(0, true);

	ht_insert(table1, "key", strdup("value"));
	ht_insert(table2, "key", strdup("value"));
	assert(table1->count == table2->count == 1);
	assert(strcmp(ht_get(table1, "key"), "value") == 0);
	assert(ht_delete(table2, "key") == NULL);
	assert(table2->count == 0);

	char *s = ht_delete(table1, "key");
	assert(strcmp(s, "value") == 0);
	free(s);

	ht_insert(table1, "statickey", "staticvalue");
	assert(strcmp(ht_delete(table1, "statickey"), "staticvalue") == 0);

	for (int i = 0; i < 10000; i++) {
		char kb[12], vb[12];
		snprintf(kb, 11, "key%d", i);
		snprintf(vb, 11, "val%d", i);
		ht_insert(table2, kb, strdup(vb));
	}
	assert(table2->count == 10000);
	assert(strcmp(ht_get(table2, "key978"), "val978") == 0);
	assert(ht_get(table2, "key-69") == NULL);

	ht_deinit(table1);
	ht_deinit(table2);
	exit(0);
}
#endif
