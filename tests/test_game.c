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

static void place_projectile(Projectile *projectile, Vector2 position,
                             Faction faction, uint32_t layer, uint32_t mask)
{
    *projectile = (Projectile) {
        .position = position,
        .faction = faction,
        .damage = 1,
        .collision_layer = layer,
        .collision_mask = mask,
        .active = true,
    };
}

static int find_room_type(const Floor *floor, RoomType type)
{
    for (int index = 0; index < floor->room_count; ++index) {
        if (floor->rooms[index].type == type) {
            return index;
        }
    }
    return -1;
}

static void enter_combat_room(Game *game)
{
    int room_index = find_room_type(&game->floor, ROOM_COMBAT);
    require_true(room_index >= 0 && game_enter_room(game, room_index),
                 "generated floor contains an enterable combat room");
}

static void test_initial_combat_room(void)
{
    Game game;
    game_init(&game);
    enter_combat_room(&game);

    require_true(game.player.health == 6, "player starts with full health");
    require_true(game_active_enemy_count(&game) == 3, "room starts with three enemies");
    require_true(!floor_current_room(&game.floor)->doors_open, "doors lock during combat");
    require_true(floor_current_room(&game.floor)->wall_count >= 2,
                 "room contains an authored wall template");
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
    enter_combat_room(&game);
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

static void test_handles_reject_previous_run(void)
{
    Game game;
    game_init(&game);
    EntityHandle old_player = game_player_handle(&game);
    EntityHandle old_enemy = {
        ENTITY_ENEMY, 0, game.enemies[0].generation, game.run_generation
    };

    game.game_over = true;
    game_restart(&game);

    require_true(!game_handle_is_valid(&game, old_player),
                 "player handle from a previous run is invalid");
    require_true(!game_handle_is_valid(&game, old_enemy),
                 "enemy handle from a previous run is invalid");
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

    require_true(floor_current_room(&game.floor)->cleared,
                 "room clears when no enemies remain");
    require_true(floor_current_room(&game.floor)->doors_open,
                 "cleared room opens its doors");
}

static void test_open_doors_are_traversable(void)
{
    Game locked;
    game_init(&locked);
    enter_combat_room(&locked);
    Room *locked_room = floor_current_room(&locked.floor);
    locked_room->door_mask |= DOOR_LEFT;
    locked.player.position = (Vector2) {
        ROOM_INSET, (float)LOGICAL_HEIGHT / 2.0f - (float)PLAYER_SIZE / 2.0f
    };
    game_update(&locked, &(GameInput) { .move_direction = { -1.0f, 0.0f } }, 0.05f);
    require_true(nearly_equal(locked.player.position.x, ROOM_INSET),
                 "locked door blocks the player");

    Game open;
    game_init(&open);
    floor_current_room(&open.floor)->door_mask |= DOOR_LEFT;
    open.player.position = locked.player.position;
    game_update(&open, &(GameInput) { .move_direction = { -1.0f, 0.0f } }, 0.05f);
    require_true(open.player.position.x < ROOM_INSET,
                 "open door permits movement into the exit corridor");
}

static void test_spawn_validation(void)
{
    Game game;
    game_init(&game);
    clear_enemies(&game);
    const Rectangle wall = floor_current_room(&game.floor)->walls[0];

    EntityHandle in_wall =
        game_spawn_enemy(&game, ENEMY_CHASER, (Vector2) { wall.x, wall.y });
    EntityHandle outside =
        game_spawn_enemy(&game, ENEMY_CHASER, (Vector2) { 0.0f, 0.0f });
    EntityHandle on_player = game_spawn_enemy(&game, ENEMY_CHASER, game.player.position);
    EntityHandle valid =
        game_spawn_enemy(&game, ENEMY_CHASER, (Vector2) { 100.0f, 100.0f });

    require_true(in_wall.kind == ENTITY_NONE, "spawn inside wall is rejected");
    require_true(outside.kind == ENTITY_NONE, "spawn outside room is rejected");
    require_true(on_player.kind == ENTITY_NONE, "spawn on player is rejected");
    require_true(game_handle_is_valid(&game, valid), "valid room spawn is accepted");
}

static void test_projectile_combat(void)
{
    Game game;
    game_init(&game);
    clear_enemies(&game);
    EntityHandle enemy =
        game_spawn_enemy(&game, ENEMY_CHASER, (Vector2) { 400.0f, 250.0f });
    int enemy_health = game.enemies[enemy.index].health;
    place_projectile(&game.projectiles[0], (Vector2) { 410.0f, 260.0f },
                     FACTION_PLAYER, COLLISION_PLAYER_SHOT,
                     COLLISION_WORLD | COLLISION_ENEMY);

    game_update(&game, &(GameInput) { 0 }, FIXED_TIMESTEP);
    require_true(game.enemies[enemy.index].health == enemy_health - 1,
                 "player projectile damages an enemy");
    require_true(!game.projectiles[0].active, "projectile is consumed on enemy hit");

    int player_health = game.player.health;
    place_projectile(&game.projectiles[0], game.player.position,
                     FACTION_ENEMY, COLLISION_ENEMY_SHOT,
                     COLLISION_WORLD | COLLISION_PLAYER);
    game_update(&game, &(GameInput) { 0 }, FIXED_TIMESTEP);
    require_true(game.player.health == player_health - 1,
                 "enemy projectile damages the player");
}

static void test_enemy_behaviors(void)
{
    Game chaser_game;
    game_init(&chaser_game);
    clear_enemies(&chaser_game);
    EntityHandle chaser =
        game_spawn_enemy(&chaser_game, ENEMY_CHASER, (Vector2) { 100.0f, 100.0f });
    Vector2 start = chaser_game.enemies[chaser.index].position;
    game_update(&chaser_game, &(GameInput) { 0 }, FIXED_TIMESTEP);
    require_true(chaser_game.enemies[chaser.index].position.x > start.x,
                 "chaser moves toward the player");

    Game spitter_game;
    game_init(&spitter_game);
    clear_enemies(&spitter_game);
    EntityHandle spitter =
        game_spawn_enemy(&spitter_game, ENEMY_SPITTER, (Vector2) { 100.0f, 100.0f });
    spitter_game.enemies[spitter.index].attack_cooldown = 0.0f;
    game_update(&spitter_game, &(GameInput) { 0 }, FIXED_TIMESTEP);
    require_true(game_active_projectile_count(&spitter_game) == 1,
                 "spitter fires an enemy projectile");
    require_true(spitter_game.projectiles[0].faction == FACTION_ENEMY,
                 "spitter projectile has enemy faction");
}

static void test_projectile_death_and_world_mask(void)
{
    Game game;
    game_init(&game);
    clear_enemies(&game);
    game.player.health = 1;
    place_projectile(&game.projectiles[0], game.player.position,
                     FACTION_ENEMY, COLLISION_ENEMY_SHOT,
                     COLLISION_WORLD | COLLISION_PLAYER);
    game_update(&game, &(GameInput) { 0 }, FIXED_TIMESTEP);
    require_true(game.game_over && game.player.health == 0,
                 "hostile projectile can cause game over");

    game_restart(&game);
    clear_enemies(&game);
    const Rectangle wall = floor_current_room(&game.floor)->walls[0];
    place_projectile(&game.projectiles[0], (Vector2) { wall.x, wall.y },
                     FACTION_PLAYER, COLLISION_PLAYER_SHOT,
                     COLLISION_WORLD | COLLISION_ENEMY);
    game_update(&game, &(GameInput) { 0 }, FIXED_TIMESTEP);
    require_true(!game.projectiles[0].active,
                 "world collision mask removes a projectile on a wall");
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
    const Rectangle wall = floor_current_room(&game.floor)->walls[0];
    game.player.position = (Vector2) {
        wall.x - (float)PLAYER_SIZE - 2.0f, wall.y
    };
    float starting_x = game.player.position.x;

    game_update(&game, &(GameInput) { .move_direction = { 1.0f, 0.0f } }, 0.05f);
    require_true(nearly_equal(game.player.position.x, starting_x),
                 "solid wall blocks player movement");
}

static void test_floor_generation_many_seeds(void)
{
    for (uint32_t seed = 1U; seed <= 256U; ++seed) {
        Floor floor;
        require_true(floor_generate(&floor, seed), "generated floor validates");
        require_true(floor_validate(&floor), "floor invariants remain valid");
        require_true(find_room_type(&floor, ROOM_START) >= 0, "floor has start room");
        require_true(find_room_type(&floor, ROOM_REWARD) >= 0, "floor has reward room");
        require_true(find_room_type(&floor, ROOM_SHOP) >= 0, "floor has shop room");
        require_true(find_room_type(&floor, ROOM_SECRET) >= 0, "floor has secret room");
        int boss = find_room_type(&floor, ROOM_BOSS);
        require_true(boss >= 0, "floor has boss room");
        int boss_distance = abs(floor.rooms[boss].grid_x) + abs(floor.rooms[boss].grid_y);
        for (int index = 1; index < floor.room_count; ++index) {
            int distance = abs(floor.rooms[index].grid_x) + abs(floor.rooms[index].grid_y);
            require_true(boss_distance >= distance,
                         "boss occupies a farthest room from the start");
        }
    }
}

static void test_floor_seed_is_deterministic(void)
{
    Floor first;
    Floor second;
    require_true(floor_generate(&first, 424242U), "first seeded floor generates");
    require_true(floor_generate(&second, 424242U), "second seeded floor generates");

    require_true(first.room_count == second.room_count, "same seed has same room count");
    for (int index = 0; index < first.room_count; ++index) {
        const Room *left = &first.rooms[index];
        const Room *right = &second.rooms[index];
        require_true(left->grid_x == right->grid_x && left->grid_y == right->grid_y,
                     "same seed reproduces room coordinates");
        require_true(left->type == right->type && left->door_mask == right->door_mask &&
                         left->template_index == right->template_index,
                     "same seed reproduces room content");
    }
}

static Direction first_connected_direction(const Room *room)
{
    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
        if ((room->door_mask & (1U << (unsigned int)direction)) != 0U) {
            return (Direction)direction;
        }
    }
    return DIRECTION_COUNT;
}

static void position_for_transition(Game *game, Direction direction, GameInput *input)
{
    *input = (GameInput) { 0 };
    if (direction == DIRECTION_LEFT) {
        game->player.position = (Vector2) { 0.0f, 250.0f };
        input->move_direction.x = -1.0f;
    } else if (direction == DIRECTION_RIGHT) {
        game->player.position = (Vector2) { LOGICAL_WIDTH - PLAYER_SIZE, 250.0f };
        input->move_direction.x = 1.0f;
    } else if (direction == DIRECTION_UP) {
        game->player.position = (Vector2) { 460.0f, 0.0f };
        input->move_direction.y = -1.0f;
    } else {
        game->player.position = (Vector2) { 460.0f, LOGICAL_HEIGHT - PLAYER_SIZE };
        input->move_direction.y = 1.0f;
    }
}

static void test_room_transition_and_persistence(void)
{
    Game game;
    game_init_with_seed(&game, 777U);
    int start_index = game.floor.current_room;
    Direction outward = first_connected_direction(floor_current_room(&game.floor));
    int neighbor = floor_neighbor(&game.floor, start_index, outward);
    GameInput input;
    position_for_transition(&game, outward, &input);
    game_update(&game, &input, FIXED_TIMESTEP);

    require_true(game.floor.current_room == neighbor, "door enters connected room");
    require_true(game.floor.rooms[neighbor].visited, "entered room becomes visible on map");
    clear_enemies(&game);
    game_update(&game, &(GameInput) { 0 }, FIXED_TIMESTEP);
    require_true(game.floor.rooms[neighbor].cleared, "entered combat can be cleared");

    Direction back = (Direction)((outward + 2) % DIRECTION_COUNT);
    position_for_transition(&game, back, &input);
    game_update(&game, &input, FIXED_TIMESTEP);
    require_true(game.floor.current_room == start_index, "opposite door returns to start");

    position_for_transition(&game, outward, &input);
    game_update(&game, &input, FIXED_TIMESTEP);
    require_true(game.floor.current_room == neighbor &&
                     game.floor.rooms[neighbor].cleared &&
                     game_active_enemy_count(&game) == 0,
                 "cleared room stays cleared after revisiting");
}

int main(void)
{
    test_initial_combat_room();
    test_fixed_update_is_repeatable();
    test_entity_handles_reject_stale_generation();
    test_handles_reject_previous_run();
    test_damage_and_invulnerability();
    test_room_clears_and_opens_doors();
    test_open_doors_are_traversable();
    test_spawn_validation();
    test_projectile_combat();
    test_enemy_behaviors();
    test_projectile_death_and_world_mask();
    test_pause_and_restart();
    test_wall_collision();
    test_floor_generation_many_seeds();
    test_floor_seed_is_deterministic();
    test_room_transition_and_persistence();
    puts("All game tests passed.");
    return EXIT_SUCCESS;
}
