#pragma once
#include <cglm/cglm.h>
#include <GL/gl3w.h>
#include <math.h>

#include "ingame.h"
#include "nuklear_custom.h"
#include "util.h"

#define PLAYER_EYE_HEIGHT 1.62
#define PLAYER_HEIGHT 1.8
#define PLAYER_JUMP_SPEED 8.7
#define PLAYER_RUN_SPEED 4.3

bool ingame_init(int initial_width, int initial_height);
void ingame_deinit(void);

void ingame_handle_input(SDL_Event* evt);
void ingame_logic(int delta_ms);

struct ingame_data_s {
    double loc[3];
    double pitch, yaw, dz;
    int picked_block[3];
    uint16_t held_block;
    uint8_t held_block_state;
    int8_t picked_block_face;

    float time_of_day, sun[3], moon[3];
    int8_t time_advance_state;
};
extern struct ingame_data_s igdt;
extern int chunk_render_radius;
