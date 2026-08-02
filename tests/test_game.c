#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "game.h"

static void require_true(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static bool nearly_equal(float left, float right)
{
    return fabsf(left - right) < 0.001f;
}

static void test_initial_state(void)
{
    Game game;
    game_init(&game);

    require_true(nearly_equal(game.player_position.x, 460.0f),
                 "player starts centered horizontally");
    require_true(nearly_equal(game.player_position.y, 250.0f),
                 "player starts centered vertically");
    require_true(game_active_projectile_count(&game) == 0,
                 "a new game has no projectiles");
    require_true(!game.paused, "a new game is not paused");
}

static void test_fixed_update_is_repeatable(void)
{
    Game first;
    Game second;
    game_init(&first);
    game_init(&second);
    GameInput input = {
        .move_direction = { .x = 1.0f, .y = 0.0f },
        .aim_direction = { .x = 0.0f, .y = -1.0f },
        .shooting = true,
    };

    for (int tick = 0; tick < 30; ++tick) {
        game_update(&first, &input, FIXED_TIMESTEP);
        game_update(&second, &input, FIXED_TIMESTEP);
    }

    require_true(nearly_equal(first.player_position.x, second.player_position.x),
                 "equal input produces equal player position");
    require_true(game_active_projectile_count(&first) ==
                     game_active_projectile_count(&second),
                 "equal input produces equal projectile count");
    require_true(game_active_projectile_count(&first) > 0,
                 "shooting input creates projectiles");
}

static void test_pause_stops_simulation(void)
{
    Game game;
    game_init(&game);
    GameInput movement = { .move_direction = { .x = 1.0f, .y = 0.0f } };
    GameInput pause = { .toggle_pause = true };
    Vector2 original_position = game.player_position;

    game_handle_actions(&game, &pause);
    game_update(&game, &movement, FIXED_TIMESTEP);

    require_true(game.paused, "pause action pauses the game");
    require_true(nearly_equal(game.player_position.x, original_position.x),
                 "paused simulation does not move the player");
    require_true(nearly_equal(game.player_position.y, original_position.y),
                 "paused simulation preserves vertical position");
}

static void test_player_stays_inside_room(void)
{
    Game game;
    game_init(&game);
    game.player_position = (Vector2) { .x = 0.0f, .y = 0.0f };
    GameInput input = { .move_direction = { .x = -1.0f, .y = -1.0f } };

    game_update(&game, &input, 1.0f);

    require_true(nearly_equal(game.player_position.x, 0.0f),
                 "player cannot leave the left edge");
    require_true(nearly_equal(game.player_position.y, 0.0f),
                 "player cannot leave the top edge");
}

int main(void)
{
    test_initial_state();
    test_fixed_update_is_repeatable();
    test_pause_stops_simulation();
    test_player_stays_inside_room();
    puts("All game tests passed.");
    return EXIT_SUCCESS;
}
