#ifndef BIND_OF_POKE_PLATFORM_H
#define BIND_OF_POKE_PLATFORM_H

#include <stdbool.h>

#include <SDL.h>

typedef struct Platform {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint64 previous_counter;
    double counter_frequency;
} Platform;

bool platform_init(Platform *platform);
void platform_shutdown(Platform *platform);
double platform_frame_time(Platform *platform);
void platform_window_to_logical(const Platform *platform, int window_x, int window_y,
                                float *logical_x, float *logical_y);

#endif
