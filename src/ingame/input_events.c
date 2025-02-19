#include "ingame.h"
#include "world.h"

static void handle_block_pick(Uint8 button)
{
	int bx = igdt.picked_block[0], by = igdt.picked_block[1], bz = igdt.picked_block[2], fx = igdt.picked_block_face;
	if (button == SDL_BUTTON_LEFT) {
		block_instance_t air = { 0 };
		world_set_block(bx, by, bz, &air);
	} else if (button == SDL_BUTTON_MIDDLE) {
		block_instance_t *inst = world_get_block(bx, by, bz);
		igdt.held_block = inst->id;
		igdt.held_block_state = inst->state;
	} else if (button == SDL_BUTTON_RIGHT) {
		block_instance_t tgt = { .id = igdt.held_block, .state = igdt.held_block_state };
		world_set_block(bx + cube_normal[fx][0], by + cube_normal[fx][1], bz + cube_normal[fx][2], &tgt);
	}
}

void ingame_handle_input(SDL_Event *evt)
{
	if (evt->type == SDL_MOUSEBUTTONDOWN) {
		if (igdt.picked_block_face != FACE_UNKNOWN)
			handle_block_pick(evt->button.button);
	} else if (evt->type == SDL_KEYUP) {
		SDL_Keycode kc = evt->key.keysym.sym;
		if (kc >= SDLK_1 && kc <= SDLK_7) {
			igdt.held_block = kc - SDLK_1 + 1;
			igdt.held_block_state = 0;
		}
	}
}
