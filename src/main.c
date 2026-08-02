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
#include "save.h"

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

    SaveData save_data;
    (void)save_data_load("bind-of-poke.save", &save_data);

    Game game;
    if (!game_init_with_seed(&game, seed)) {
        fprintf(stderr, "Floor generation failed for seed %u.\n", seed);
        platform_shutdown(&platform);
        return 1;
    }
    game.completed_runs = save_data.completed_runs;
    game.reduced_flashes = save_data.reduced_flashes;
    platform_set_seed_title(&platform, game.floor.seed);

    InputSystem input_system;
    input_init(&input_system);

    bool running = true;
    double accumulator = 0.0;
    bool victory_saved = false;
    double save_retry_timer = 0.0;

    while (running) {
        double frame_time = platform_frame_time(&platform);
        accumulator += frame_time;
        if (save_retry_timer > 0.0) {
            save_retry_timer -= frame_time;
        }

        AppInput input;
        input_poll(&input_system, &input, &platform, &game);
        if (input.quit_requested) {
            running = false;
        }
        bool reduced_flashes_before = game.reduced_flashes;
        game_handle_actions(&game, &input.game);
        if (reduced_flashes_before != game.reduced_flashes) {
            save_data.reduced_flashes = game.reduced_flashes;
            if (!save_data_write("bind-of-poke.save", &save_data)) {
                fprintf(stderr, "Could not save accessibility setting.\n");
            }
        }

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

        if (game.victory && !victory_saved && save_retry_timer <= 0.0) {
            save_data.completed_runs = game.completed_runs;
            save_data.boss_defeated = true;
            victory_saved = save_data_write("bind-of-poke.save", &save_data);
            if (!victory_saved) {
                fprintf(stderr, "Could not write save data.\n");
                save_retry_timer = 2.0;
            }
        } else if (!game.victory) {
            victory_saved = false;
            save_retry_timer = 0.0;
        }

        renderer_draw(platform.renderer, &game);
    }

    input_shutdown(&input_system);
    platform_shutdown(&platform);
    return 0;
}
