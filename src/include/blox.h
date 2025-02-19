#pragma once

#include <stdbool.h>
#include <stdint.h>
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define UNUSED(A) (void)(A)
#define __STRING_IFY(x) #x
#define STRINGIFY(x) __STRING_IFY(x)
#define INITIAL_SCREEN_WIDTH 1070
#define INITIAL_SCREEN_HEIGHT 600

#ifndef M_PI
#define M_PI 3.14159265358979323
#endif

enum {
    FACE_UNKNOWN = -1,
    FACE_UP = 0,
    FACE_DOWN,
    FACE_NORTH,
    FACE_SOUTH,
    FACE_EAST,
    FACE_WEST,
    FACE_MAX
};

extern bool doQuit;
extern int8_t cube_normal[FACE_MAX][3];

static inline int64_t pack32(int32_t a, int32_t b)
{
    union {
        struct {
            int32_t a;
            int32_t b;
        } unpacked;
        int64_t packed;
    } machine;
    machine.unpacked.a = a;
    machine.unpacked.b = b;
    return machine.packed;
}
