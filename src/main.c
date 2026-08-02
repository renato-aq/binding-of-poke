#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "game.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"

enum { MAX_UPDATES_PER_FRAME = 8 };

static bool parse_seed(const char *text, uint32_t *seed)
{
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX) {
        return false;
    }
    *seed = (uint32_t)value;
    return true;
}

int main(int argument_count, char *arguments[])
{
    uint32_t seed = 0xB10D2026U;
    if (argument_count > 2 ||
        (argument_count == 2 && !parse_seed(arguments[1], &seed))) {
        fprintf(stderr, "Usage: %s [unsigned-seed]\n", arguments[0]);
        return 1;
    }

    Platform platform;
    if (!platform_init(&platform)) {
        return 1;
    }

    Game game;
    if (!game_init_with_seed(&game, seed)) {
        fprintf(stderr, "Floor generation failed for seed %u.\n", seed);
        platform_shutdown(&platform);
        return 1;
    }
    platform_set_seed_title(&platform, game.floor.seed);

    bool running = true;
    double accumulator = 0.0;

    while (running) {
        accumulator += platform_frame_time(&platform);

        AppInput input;
        input_poll(&input, &platform, &game);
        if (input.quit_requested) {
            running = false;
        }
        game_handle_actions(&game, &input.game);

        int update_count = 0;
        while (accumulator >= (double)FIXED_TIMESTEP &&
               update_count < MAX_UPDATES_PER_FRAME) {
            game_update(&game, &input.game, FIXED_TIMESTEP);
            accumulator -= (double)FIXED_TIMESTEP;
            ++update_count;
        }
        if (update_count == MAX_UPDATES_PER_FRAME) {
            accumulator = 0.0;
        }

        renderer_draw(platform.renderer, &game);
    }

    platform_shutdown(&platform);
    return 0;
}
