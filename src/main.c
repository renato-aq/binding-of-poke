#include <stdbool.h>
#include <stdio.h>

#include "config.h"
#include "game.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"

enum { MAX_UPDATES_PER_FRAME = 8 };

int main(void)
{
    Platform platform;
    if (!platform_init(&platform)) {
        return 1;
    }

    Game game;
    game_init(&game);

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
