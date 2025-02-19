#include <assert.h>
#include <cJSON.h>
#include <physfs.h>
#include "util.h"
#include "world.h"

typedef struct model_js {
	char *text;
	cJSON *js;
	struct model_js *prev, *next;
} model_js_t;

typedef struct btexrep_s {
	char *name;
	int texindex;
	SDL_Surface *image;
	/* A pointer to a texture info struct might go here. After btexrep is retired, it will go into
	 * an array and be indexed by texindex. */
} btexrep_t;

#define CJSON_SET_INT(JSCONTAINER, OBJCONTAINER, KEY)                                                                                     \
	do {                                                                                                                              \
		const cJSON *cjsonsetintpt = cJSON_GetObjectItem((JSCONTAINER), STRINGIFY(KEY));                                          \
		if (cJSON_IsNumber(cjsonsetintpt))                                                                                        \
			(OBJCONTAINER)->KEY = cjsonsetintpt->valueint;                                                                    \
	} while (0);
#define CJSON_SET_BOOL(JSCONTAINER, OBJCONTAINER, KEY)                                                                                    \
	do {                                                                                                                              \
		const cJSON *cjsonsetbpt = cJSON_GetObjectItem((JSCONTAINER), STRINGIFY(KEY));                                            \
		if (cJSON_IsBool(cjsonsetbpt))                                                                                            \
			(OBJCONTAINER)->KEY = cJSON_IsTrue(cjsonsetbpt);                                                                  \
	} while (0);
#define MODELJS_GET_PROP(TARGETVAR, LISTTAIL, KEY)                                                                                        \
	do {                                                                                                                              \
		TARGETVAR = NULL;                                                                                                         \
		for (model_js_t *it = (LISTTAIL); (TARGETVAR) == NULL && it != NULL; it = it->prev) {                                     \
			if (cJSON_HasObjectItem(it->js, (KEY)))                                                                           \
				TARGETVAR = cJSON_GetObjectItem(it->js, (KEY));                                                           \
		}                                                                                                                         \
	} while (0);

GLuint block_textures = 0;
blockdef_t *blockdefs = NULL;
blockstate_t default_state = { 0, .opaque = true, .pickable = true };

static btexrep_t *create_btexrep(const char *path, int tex_size)
{
	char edpbuf[512];
	SDL_Surface *surf = load_physfs_image(path);
	if (surf == NULL || surf->w != tex_size || surf->h != tex_size) {
		if (surf)
			SDL_FreeSurface(surf);
		return NULL;
	}

	strncpy(edpbuf, path + 20, 511);
	char *name_ext = strrchr(edpbuf, '.');
	if (name_ext)
		*name_ext = 0;

	btexrep_t *r = malloc(sizeof(btexrep_t));
	r->name = strdup(edpbuf);
	r->image = surf;

	snprintf(edpbuf, 512, "%s.json", path);
	if (PHYSFS_exists(edpbuf)) {
		/* Load extradata. */
	}

	return r;
}

static model_info_t *load_model(const char *model_name, htable_t *texture_lookup)
{
	model_js_t *model_head = NULL, *model_tail;

	const char *parent_model_name = model_name;
	do {
		char pathbuf[200];
		snprintf(pathbuf, 199, "/resources/models/%s.json", parent_model_name);

		model_js_t *new_model = malloc(sizeof(model_js_t));
		new_model->text = read_physfs_file(pathbuf, NULL);
		new_model->js = cJSON_Parse(new_model->text);
		if (new_model->js == NULL) {
			fprintf(stderr, "FATAL: Referenced model %s contains invalid JSON.\n", model_name);
			abort();
		}
		if (model_head)
			model_head->prev = new_model;
		new_model->prev = NULL;
		new_model->next = model_head;
		model_head = new_model;
		if (cJSON_IsString(cJSON_GetObjectItem(model_head->js, "parent")))
			parent_model_name = cJSON_GetStringValue(cJSON_GetObjectItem(model_head->js, "parent"));
		else
			parent_model_name = NULL;
	} while (parent_model_name != NULL);

	/* Get the last element, so it's possible to iterate in reverse order. */
	model_tail = model_head;
	while (model_tail->next)
		model_tail = model_tail->next;

	/* Ensure that elements are present. Save the last one. */
	cJSON *elements_js;
	MODELJS_GET_PROP(elements_js, model_tail, "elements");
	if (elements_js == NULL || cJSON_IsArray(elements_js) == false) {
		fprintf(stderr, "FATAL: Referenced model %s and its ancestors don't define any elements.\n", model_name);
		abort();
	}

	int num_elements = cJSON_GetArraySize(elements_js);
	model_info_t *mdl = malloc(sizeof(model_info_t) + num_elements * sizeof(model_element_t));
	mdl->num_elements = num_elements;

	/* Load texture variables. */
	htable_t *texture_vars = ht_init(12, true);
	for (model_js_t *i = model_head; i != NULL; i = i->next) {
		cJSON *j;
		if (!cJSON_IsObject(cJSON_GetObjectItem(i->js, "textures")))
			continue;
		cJSON_ArrayForEach(j, cJSON_GetObjectItem(i->js, "textures"))
		{
			assert(cJSON_IsString(j));
			ht_insert(texture_vars, j->string, strdup(j->valuestring));
		}
	}

	cJSON *cull_neighbor_js;
	MODELJS_GET_PROP(cull_neighbor_js, model_tail, "cull_neighbors");
	if (cJSON_IsArray(cull_neighbor_js)) {
		cJSON *cn_js_it;
		cJSON_ArrayForEach(cn_js_it, cull_neighbor_js)
		{
			int face_index = FACE_NAME_INDEX(cJSON_GetStringValue(cn_js_it));
			if (face_index >= 0)
				mdl->cull_neighbors |= (1 << face_index);
		}
	}

	/* Process elements last. */
	cJSON *element_js;
	model_element_t *mdlel = mdl->elements;
	cJSON_ArrayForEach(element_js, elements_js)
	{
		cJSON *cube_from = cJSON_GetObjectItem(element_js, "from"), *cube_to = cJSON_GetObjectItem(element_js, "to"), *cube_dim_el;
		{
			int i = 0;
			assert(cJSON_IsArray(cube_from) && cJSON_GetArraySize(cube_from) == 3);
			assert(cJSON_IsArray(cube_to) && cJSON_GetArraySize(cube_to) == 3);
			cJSON_ArrayForEach(cube_dim_el, cube_from)
			{
				mdlel->cube[i++] = cube_dim_el->valuedouble;
			}
			cJSON_ArrayForEach(cube_dim_el, cube_to)
			{
				mdlel->cube[i++] = cube_dim_el->valuedouble;
			}
		}

		cJSON *cube_faces = cJSON_GetObjectItem(element_js, "faces"), *cube_face_js;
		assert(cJSON_IsObject(cube_faces));
		cJSON_ArrayForEach(cube_face_js, cube_faces)
		{
			int face_index = FACE_NAME_INDEX(cube_face_js->string);
			if (face_index >= 0)
				mdlel->faces |= (1 << face_index);
			else
				continue;

			btexrep_t *tex;
			if (cJSON_IsString(cJSON_GetObjectItem(cube_face_js, "texture"))) {
				const char *tex_name = cJSON_GetStringValue(cJSON_GetObjectItem(cube_face_js, "texture"));
				while (tex_name != NULL && tex_name[0] == '#')
					tex_name = ht_get(texture_vars, tex_name + 1);

				tex = tex_name == NULL ? NULL : ht_get(texture_lookup, tex_name);
				if (tex == NULL)
					tex = ht_get(texture_lookup, "blocks/missing");
			} else {
				tex = ht_get(texture_lookup, "blocks/missing");
			}
			mdlel->textures[face_index] = tex->texindex;

			cJSON *face_uv_js = cJSON_GetObjectItem(cube_face_js, "uv");
			if (cJSON_IsArray(face_uv_js)) {
				int i = 0;
				cJSON *face_uvx_js;
				assert(cJSON_GetArraySize(face_uv_js) == 4);
				cJSON_ArrayForEach(face_uvx_js, face_uv_js)
				{
					assert(cJSON_IsNumber(face_uvx_js));
					mdlel->uv[face_index][i++] = face_uvx_js->valuedouble;
				}
			} else {
				for (int i = 0; i < 4; i++)
					mdlel->uv[face_index][i] = i >> 1;
			}

			if (cJSON_IsTrue(cJSON_GetObjectItem(cube_face_js, "cull_self")))
				mdlel->cull_faces |= 1 << face_index;
		}

		mdlel++;
	}

	for (model_js_t *it = model_head; it != NULL;) {
		model_js_t *c = it;
		it = it->next;

		cJSON_Delete(c->js);
		free(c->text);
		free(c);
	}

	ht_deinit(texture_vars);
	return mdl;
}

static bool string_ends_with(const char *s, const char *suffix)
{
	if (s == NULL || suffix == NULL)
		return false;

	int s_len = strlen(s), suffix_len = strlen(suffix);
	if (suffix_len > s_len)
		return false;
	return strncmp(s + s_len - suffix_len, suffix, suffix_len) == 0;
}

static htable_t *world_load_textures(void)
{
	htable_t *texture_lookup = ht_init(50, true);
	PHYSFS_Stat stat;
	char pathbuf[512];

	void *tdimtxt = read_physfs_file("/resources/textures/blocks/tex_size.txt", NULL);
	int tex_size = tdimtxt ? atoi(tdimtxt) : 16;
	if (tex_size == 0)
		tex_size = 16;
	free(tdimtxt);

	char **tld = PHYSFS_enumerateFiles("/resources/textures/blocks"), **i;
	for (i = tld; *i; i++) {
		snprintf(pathbuf, 512, "/resources/textures/blocks/%s", *i);
		if (PHYSFS_stat(pathbuf, &stat) == 0)
			continue;
		if (stat.filetype == PHYSFS_FILETYPE_REGULAR && string_ends_with(*i, ".png")) {
			btexrep_t *r = create_btexrep(pathbuf, tex_size);
			if (r)
				ht_insert(texture_lookup, r->name, r);
		}
	}
	PHYSFS_freeList(tld);

	/* Insert a missing texture, and scale it to the other block textures' size. */
	SDL_Surface *missing_surf, *raw_missing_surf = load_physfs_image("/resources/textures/missing.png");
	if (raw_missing_surf->w != tex_size || raw_missing_surf->h != tex_size) {
		missing_surf = SDL_CreateRGBSurface(0, tex_size, tex_size, 32, 0xFF << 24, 0xFF << 16, 0xFF << 8, 0xFF << 0);
		SDL_BlitScaled(raw_missing_surf, NULL, missing_surf, NULL);
		SDL_FreeSurface(raw_missing_surf);
	} else
		missing_surf = raw_missing_surf;
	btexrep_t *missing_r = malloc(sizeof(btexrep_t));
	missing_r->name = strdup("blocks/missing"); /* allow it to be freed later */
	missing_r->image = missing_surf;
	ht_insert(texture_lookup, missing_r->name, missing_r);

	/* Load block textures into hardware. Simultaneously assign texture indexes based on hash table position. */
	glGenTextures(1, &block_textures);
	glBindTexture(GL_TEXTURE_2D_ARRAY, block_textures);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, tex_size, tex_size, texture_lookup->count);
	for (int i = 0, tx = 0; i < texture_lookup->buckets; i++) {
		if (texture_lookup->data[i] == NULL)
			continue;

		btexrep_t *r = texture_lookup->data[i]->value;
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, tx, tex_size, tex_size, 1, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
				r->image->pixels);
		r->texindex = tx++;
		SDL_FreeSurface(r->image);
	}
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	return texture_lookup;
}

static void calculate_light_effective_range(float l[4])
{
	const double min_light = 0.002, inv_min_light = 1 / min_light;
	float a = l[2], b = l[1], c = l[0] - inv_min_light;
	l[3] = 1 + (-b + sqrtf(b * b - 4 * a * c)) / (2 * a);
	/* Add 1 because each point light source is a cube of size 1 */
}

static void world_load_blocks(htable_t *texture_lookup)
{
	htable_t *models_by_name;
	char *block_list = read_physfs_file("/resources/blocks/_all_blocks.txt", NULL), *block_list_saveptr, *block_name;
	int n_block_types = 0, block_index;
	for (char *s = block_list; *s; s++) {
		if (*s == '\r' || *s == '\n') {
			n_block_types++;
			while (*(s + 1) == '\r' || *(s + 1) == '\n')
				s++;
		}
	}
	blockdefs = malloc((n_block_types + 1) * sizeof(blockdef_t));
	models_by_name = ht_init(n_block_types, false);

	/* Initialize the air block definition -- all properties happen to be zero, and no model */
	blockdefs[0].name = NULL;
	blockdefs[0].num_states = 1;
	blockdefs[0].states = calloc(1, sizeof(blockstate_t));

	block_name = strtok_r(block_list, "\r\n", &block_list_saveptr);
	block_index = 1;
	while (block_name != NULL) {
		char pathbuf[150], *jstext;
		cJSON *root, *state_list, *statejs, *it;
		snprintf(pathbuf, 149, "/resources/blocks/%s.json", block_name);
		jstext = read_physfs_file(pathbuf, NULL);
		if ((root = cJSON_Parse(jstext)) == NULL) {
			fprintf(stderr, "FATAL: Block definition %s is not valid JSON\n", pathbuf);
			abort();
		}

		state_list = cJSON_GetObjectItem(root, "states");
		assert(cJSON_IsArray(state_list));
		blockdefs[block_index].name = strdup(block_name);
		blockdefs[block_index].num_states = cJSON_GetArraySize(state_list);
		blockdefs[block_index].states = calloc(blockdefs[block_index].num_states, sizeof(blockstate_t));

		blockstate_t *curr_state = blockdefs[block_index].states;
		cJSON_ArrayForEach(statejs, state_list)
		{
			/* Apply defaults. All other values are zero or false. */
			curr_state->pickable = true;
			curr_state->opaque = true;

			assert(cJSON_IsObject(statejs));
			it = cJSON_GetObjectItem(statejs, "model");
			if (cJSON_IsString(it)) {
				const char *mdl_name = cJSON_GetStringValue(it);
				model_info_t *mdl = ht_get(models_by_name, mdl_name);
				if (mdl == NULL) {
					mdl = load_model(mdl_name, texture_lookup);
					ht_insert(models_by_name, mdl_name, mdl);
				}
				curr_state->model = mdl;
			}

			it = cJSON_GetObjectItem(statejs, "light_source");
			if (cJSON_IsObject(it)) {
				int failure = 0;
				cJSON *light_strength = cJSON_GetObjectItem(it, "luminosity"), *color = cJSON_GetObjectItem(it, "color");
				if (cJSON_IsArray(light_strength) && cJSON_IsArray(color) && cJSON_GetArraySize(light_strength) == 3 &&
				    cJSON_GetArraySize(color) == 3) {
					int i = 0;
					cJSON *it2;
					cJSON_ArrayForEach(it2, light_strength)
					{
						if (cJSON_IsNumber(it2))
							curr_state->pointlight.luminosity[i++] = it2->valuedouble;
						else
							break;
					}
					calculate_light_effective_range(curr_state->pointlight.luminosity);

					i = 0;
					cJSON_ArrayForEach(it2, color)
					{
						if (cJSON_IsNumber(it2))
							curr_state->pointlight.color[i++] = it2->valueint & 0xFF;
						else
							break;
					}
				}

				if (failure)
					curr_state->pointlight.luminosity[0] = 0;
			} else
				curr_state->pointlight.luminosity[0] = 0;

			CJSON_SET_INT(statejs, curr_state, fluid);
			CJSON_SET_BOOL(statejs, curr_state, pickable);
			CJSON_SET_BOOL(statejs, curr_state, opaque);
			CJSON_SET_BOOL(statejs, curr_state, translucent);
			CJSON_SET_BOOL(statejs, curr_state, scatter_skylight);

			curr_state++;
		}

		cJSON_Delete(root);
		free(jstext);
		block_name = strtok_r(NULL, "\r\n", &block_list_saveptr);
		block_index++;
	}

	free(block_list);
	ht_deinit(models_by_name);
}

void world_load_resources(void)
{
	htable_t *texture_lookup = world_load_textures();
	world_load_blocks(texture_lookup);

	for (int i = 0; i < texture_lookup->buckets; i++) {
		if (texture_lookup->data[i])
			free(((btexrep_t *)texture_lookup->data[i]->value)->name);
	}
	ht_deinit(texture_lookup);
}
