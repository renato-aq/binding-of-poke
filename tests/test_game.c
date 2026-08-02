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

static void clear_enemies(Game *game)
{
    for (int index = 0; index < MAX_ENEMIES; ++index) {
        game->enemies[index].active = false;
    }
}

static void test_initial_combat_room(void)
{
    Game game;
    game_init(&game);

    require_true(game.player.health == 6, "player starts with full health");
    require_true(game_active_enemy_count(&game) == 3, "room starts with three enemies");
    require_true(!game.room.doors_open, "doors lock during combat");
    require_true(game.room.wall_count == 3, "room contains authored walls");
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

    require_true(nearly_equal(first.player.position.x, second.player.position.x),
                 "equal input produces equal player position");
    require_true(game_active_projectile_count(&first) ==
                     game_active_projectile_count(&second),
                 "equal input produces equal projectile count");
}

static void test_entity_handles_reject_stale_generation(void)
{
    Game game;
    game_init(&game);
    clear_enemies(&game);
    EntityHandle old = game_spawn_enemy(&game, ENEMY_CHASER, (Vector2) { 100.0f, 100.0f });

    require_true(game_handle_is_valid(&game, old), "new enemy handle is valid");
    game_apply_damage(&game, old, FACTION_PLAYER, 99);
    require_true(!game_handle_is_valid(&game, old), "dead enemy handle is invalid");

    EntityHandle replacement =
        game_spawn_enemy(&game, ENEMY_SPITTER, (Vector2) { 100.0f, 100.0f });
    require_true(replacement.index == old.index, "enemy pool reuses free slots");
    require_true(replacement.generation != old.generation,
                 "reused slots receive a new generation");
    require_true(!game_handle_is_valid(&game, old), "stale handle remains invalid");
}

static void test_damage_and_invulnerability(void)
{
    Game game;
    game_init(&game);
    EntityHandle player = game_player_handle(&game);

    require_true(game_apply_damage(&game, player, FACTION_ENEMY, 1),
                 "enemy damage reaches the player");
    require_true(game.player.health == 5, "damage removes player health");
    require_true(!game_apply_damage(&game, player, FACTION_ENEMY, 1),
                 "invulnerability rejects immediate repeated damage");
    require_true(game.player.health == 5, "rejected damage preserves health");
    require_true(!game_apply_damage(&game, player, FACTION_PLAYER, 1),
                 "friendly fire is rejected");
}

static void test_room_clears_and_opens_doors(void)
{
    Game game;
    game_init(&game);
    clear_enemies(&game);

    game_update(&game, &(GameInput) { 0 }, FIXED_TIMESTEP);

    require_true(game.room.cleared, "room clears when no enemies remain");
    require_true(game.room.doors_open, "cleared room opens its doors");
}

static void test_pause_and_restart(void)
{
    Game game;
    game_init(&game);
    GameInput pause = { .toggle_pause = true };
    Vector2 original = game.player.position;

    game_handle_actions(&game, &pause);
    game_update(&game, &(GameInput) { .move_direction = { 1.0f, 0.0f } }, FIXED_TIMESTEP);
    require_true(nearly_equal(game.player.position.x, original.x),
                 "pause stops simulation");

    game.game_over = true;
    game.player.health = 0;
    game_handle_actions(&game, &(GameInput) { .restart = true });
    require_true(!game.game_over && game.player.health == game.player.maximum_health,
                 "restart creates a fresh combat room");
}

static void test_wall_collision(void)
{
    Game game;
    game_init(&game);
    clear_enemies(&game);
    game.player.position = (Vector2) { 210.0f, 220.0f };

    game_update(&game, &(GameInput) { .move_direction = { 1.0f, 0.0f } }, 0.2f);
    require_true(nearly_equal(game.player.position.x, 210.0f),
                 "solid wall blocks player movement");
}

int main(void)
{
    test_initial_combat_room();
    test_fixed_update_is_repeatable();
    test_entity_handles_reject_stale_generation();
    test_damage_and_invulnerability();
    test_room_clears_and_opens_doors();
    test_pause_and_restart();
    test_wall_collision();
    puts("All game tests passed.");
    return EXIT_SUCCESS;
}
