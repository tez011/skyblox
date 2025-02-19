#include "ingame.h"
#include "world.h"

static inline void load_chunks(int render_radius)
{
	int center_x, center_y, load_radius = render_radius + 1;
	WORLD_CHUNK(igdt.loc[0], &center_x, NULL);
	WORLD_CHUNK(igdt.loc[1], &center_y, NULL);

	for (int rx = -load_radius; rx <= load_radius; rx++) {
		for (int ry = -load_radius; ry <= load_radius; ry++) {
			chunk_t *chunk = chunks_get(center_x + rx, center_y + ry);
			if (chunk == NULL)
				world_request_chunkgen(center_x + rx, center_y + ry);
			else if (abs(rx) <= render_radius && abs(ry) <= render_radius) {
				chunk_render(chunk);
			}
		}
	}
}

static void sun_params(vec3 sun, float latitude, float longitude, int day_of_year, float time_of_day)
{
	float solar_time = time_of_day +
			   (0.170f * sinf(4 * M_PI * (day_of_year - 80) / 373) - 0.129f * sinf(2 * M_PI * (day_of_year - 8) / 355)) +
			   (longitude / 15 - rintf(longitude / 15));
	float solar_declination = 0.4093f * sinf(2 * M_PI * (day_of_year - 81) / 368);
	float lat_rads = latitude * M_PI / 180;

	sun[0] = cosf(solar_declination) * sinf(M_PI * solar_time / 12);
	sun[1] = cosf(lat_rads) * sinf(solar_declination) + sinf(lat_rads) * cosf(solar_declination) * cosf(M_PI * solar_time / 12);
	sun[2] = sinf(lat_rads) * sinf(solar_declination) - cosf(lat_rads) * cosf(solar_declination) * cosf(M_PI * solar_time / 12);
}

static void find_picked_block(void)
{
	vec3 u, v, w;
	float t = 0;
	int b[3];
	orientation_from_angles(v, igdt.pitch, igdt.yaw);
	for (int i = 0; i < 3; i++) {
		u[i] = igdt.loc[i] + (i == 2 ? PLAYER_EYE_HEIGHT : 0);
		w[i] = u[i];
	}

	igdt.picked_block_face = FACE_UNKNOWN;
	while (t < CHUNK_WIDTH) {
		float dt = -1;
		int dti = -1;
		for (int i = 0; i < 3; i++) {
			if (v[i] == 0)
				continue;
			float dtc = ((copysignf(1, v[i]) >= 0 ? floorf(w[i] + 1) : ceilf(w[i] - 1)) - w[i]) / v[i];
			if (dt == -1 || dtc < dt) {
				dt = dtc;
				dti = i;
			}
		}
		if (dti == -1)
			abort();

		t += dt;
		glm_vec3_copy(u, w);
		glm_vec3_muladds(v, t, w);

		for (int i = 0; i < 3; i++) {
			/* Coerce numbers that should be integers but aren't because of floating-point operations to actual
			 * integers. We do this because if floating-point errors build up, the min-values above will be
			 * lower than epsilon (1e-5), which prevents t from updating. */
			if (fabsf(w[i] - roundf(w[i])) < 1e-5)
				w[i] = roundf(w[i]);

			if (copysignf(1, v[dti]) >= 0)
				b[i] = (int)floorf(w[i]);
			else
				b[i] = (int)ceilf(w[i] - 1);
		}

		block_instance_t *bi = world_get_block(b[0], b[1], b[2]);
		if (bi && blockdefs[bi->id].states[bi->state].pickable) {
			for (int i = 0; i < 3; i++)
				igdt.picked_block[i] = b[i];
			igdt.picked_block_face = 2 * (2 - dti) + (copysignf(1, v[dti]) == 1);
			break;
		}
	}
}

static void player_navigate(int delta_ms)
{
	const Uint8 *kbstate = SDL_GetKeyboardState(NULL);
	const double player_speed = PLAYER_RUN_SPEED;
	vec3 facing, sideface, motion = GLM_VEC3_ZERO_INIT;

	orientation_from_angles(facing, igdt.pitch, igdt.yaw);
	facing[2] = 0;
	glm_vec3_normalize(facing);
	glm_vec3_cross(facing, GLM_ZUP, sideface);

	if (kbstate[SDL_SCANCODE_W])
		glm_vec3_add(motion, facing, motion);
	if (kbstate[SDL_SCANCODE_S])
		glm_vec3_sub(motion, facing, motion);
	if (kbstate[SDL_SCANCODE_A])
		glm_vec3_sub(motion, sideface, motion);
	if (kbstate[SDL_SCANCODE_D])
		glm_vec3_add(motion, sideface, motion);

	glm_vec3_scale_as(motion, player_speed * delta_ms / 1000.0, motion);
	igdt.loc[0] += motion[0];
	igdt.loc[1] += motion[1];

	if (kbstate[SDL_SCANCODE_SPACE])
		igdt.loc[2] += PLAYER_RUN_SPEED * delta_ms / 1000.0;
	if (kbstate[SDL_SCANCODE_LCTRL])
		igdt.loc[2] -= PLAYER_RUN_SPEED * delta_ms / 1000.0;
}

void ingame_logic(int delta_ms)
{
	load_chunks(chunk_render_radius);
	find_picked_block();
	player_navigate(delta_ms);

	igdt.time_of_day = fmodf(igdt.time_of_day + delta_ms * 24.f / 60000, 24.f);
	sun_params(igdt.sun, 42.346356, -71.097624, 181, igdt.time_of_day);
	sun_params(igdt.moon, 42.346356, -71.097624, 181, fmod(igdt.time_of_day + 11, 24));
}
