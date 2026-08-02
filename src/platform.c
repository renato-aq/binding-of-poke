#include "platform.h"

#include <stdio.h>

#include "config.h"

bool platform_init(Platform *platform)
{
    *platform = (Platform) { 0 };

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }

    platform->window = SDL_CreateWindow(
        "Bind of Poke - Movement and Shooting Prototype",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LOGICAL_WIDTH,
        LOGICAL_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (platform->window == NULL) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        platform_shutdown(platform);
        return false;
    }

    platform->renderer = SDL_CreateRenderer(
        platform->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (platform->renderer == NULL) {
        platform->renderer = SDL_CreateRenderer(platform->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (platform->renderer == NULL) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        platform_shutdown(platform);
        return false;
    }

    if (SDL_RenderSetLogicalSize(platform->renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT) != 0) {
        fprintf(stderr, "Logical resolution setup failed: %s\n", SDL_GetError());
        platform_shutdown(platform);
        return false;
    }
    SDL_RenderSetIntegerScale(platform->renderer, SDL_FALSE);

    platform->previous_counter = SDL_GetPerformanceCounter();
    platform->counter_frequency = (double)SDL_GetPerformanceFrequency();
    return true;
}

void platform_shutdown(Platform *platform)
{
    if (platform->renderer != NULL) {
        SDL_DestroyRenderer(platform->renderer);
        platform->renderer = NULL;
    }
    if (platform->window != NULL) {
        SDL_DestroyWindow(platform->window);
        platform->window = NULL;
    }
    SDL_Quit();
}

double platform_frame_time(Platform *platform)
{
    Uint64 current_counter = SDL_GetPerformanceCounter();
    double frame_time =
        (double)(current_counter - platform->previous_counter) / platform->counter_frequency;
    platform->previous_counter = current_counter;

    if (frame_time > 0.25) {
        frame_time = 0.25;
    }

    return frame_time;
}

void platform_window_to_logical(const Platform *platform, int window_x, int window_y,
                                float *logical_x, float *logical_y)
{
    SDL_RenderWindowToLogical(platform->renderer, window_x, window_y, logical_x, logical_y);
}

void platform_set_seed_title(Platform *platform, uint32_t seed)
{
    char title[96];
    (void)snprintf(title, sizeof(title), "Bind of Poke - Floor Seed: %u", seed);
    SDL_SetWindowTitle(platform->window, title);
}
