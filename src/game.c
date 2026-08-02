#include "game.h"

#include <math.h>

static const float PLAYER_SPEED = 260.0f;
static const float CHASER_SPEED = 92.0f;
static const float SPITTER_SPEED = 48.0f;
static const float PLAYER_PROJECTILE_SPEED = 520.0f;
static const float ENEMY_PROJECTILE_SPEED = 250.0f;
static const float PLAYER_SHOT_COOLDOWN = 0.16f;
static const float SPITTER_SHOT_COOLDOWN = 1.35f;
static const float PLAYER_INVULNERABILITY = 0.75f;
static const float DOOR_HALF_WIDTH = 45.0f;
static const float BOMB_FUSE = 0.8f;
static const float BOMB_RADIUS = 105.0f;

static bool rectangles_overlap(Rectangle left, Rectangle right)
{
    return left.x < right.x + right.width && left.x + left.width > right.x &&
           left.y < right.y + right.height && left.y + left.height > right.y;
}

static Vector2 normalized(Vector2 vector)
{
    float length_squared = vector.x * vector.x + vector.y * vector.y;
    if (length_squared <= 0.0f) {
        return (Vector2) { 0 };
    }
    float inverse_length = 1.0f / sqrtf(length_squared);
    return (Vector2) { vector.x * inverse_length, vector.y * inverse_length };
}

static Rectangle player_bounds(const Game *game)
{
    return (Rectangle) {
        game->player.position.x, game->player.position.y, PLAYER_SIZE, PLAYER_SIZE
    };
}

static Rectangle enemy_bounds(const Enemy *enemy)
{
    return (Rectangle) { enemy->position.x, enemy->position.y, ENEMY_SIZE, ENEMY_SIZE };
}

static Rectangle projectile_bounds(const Projectile *projectile)
{
    return (Rectangle) {
        projectile->position.x, projectile->position.y, PROJECTILE_SIZE, PROJECTILE_SIZE
    };
}

static bool collides_with_world(const Game *game, Rectangle bounds)
{
    const Room *room = floor_current_room_const(&game->floor);
    if (bounds.x < 0.0f || bounds.y < 0.0f ||
        bounds.x + bounds.width > LOGICAL_WIDTH ||
        bounds.y + bounds.height > LOGICAL_HEIGHT) {
        return true;
    }

    bool inside_horizontal_door =
        bounds.x >= (float)LOGICAL_WIDTH / 2.0f - DOOR_HALF_WIDTH &&
        bounds.x + bounds.width <= (float)LOGICAL_WIDTH / 2.0f + DOOR_HALF_WIDTH;
    bool inside_vertical_door =
        bounds.y >= (float)LOGICAL_HEIGHT / 2.0f - DOOR_HALF_WIDTH &&
        bounds.y + bounds.height <= (float)LOGICAL_HEIGHT / 2.0f + DOOR_HALF_WIDTH;

    if (bounds.x < ROOM_INSET &&
        !(room->doors_open &&
          floor_connection_revealed(&game->floor, game->floor.current_room, DIRECTION_LEFT) &&
          inside_vertical_door)) {
        return true;
    }
    if (bounds.x + bounds.width > LOGICAL_WIDTH - ROOM_INSET &&
        !(room->doors_open &&
          floor_connection_revealed(&game->floor, game->floor.current_room, DIRECTION_RIGHT) &&
          inside_vertical_door)) {
        return true;
    }
    if (bounds.y < ROOM_INSET &&
        !(room->doors_open &&
          floor_connection_revealed(&game->floor, game->floor.current_room, DIRECTION_UP) &&
          inside_horizontal_door)) {
        return true;
    }
    if (bounds.y + bounds.height > LOGICAL_HEIGHT - ROOM_INSET &&
        !(room->doors_open &&
          floor_connection_revealed(&game->floor, game->floor.current_room, DIRECTION_DOWN) &&
          inside_horizontal_door)) {
        return true;
    }

    for (int index = 0; index < room->wall_count; ++index) {
        if (rectangles_overlap(bounds, room->walls[index])) {
            return true;
        }
    }
    return false;
}

static void move_with_world_collision(const Game *game, Vector2 *position, float size,
                                      Vector2 velocity, float delta_time)
{
    Vector2 original = *position;
    position->x += velocity.x * delta_time;
    Rectangle bounds = { position->x, position->y, size, size };
    if (collides_with_world(game, bounds)) {
        position->x = original.x;
    }

    position->y += velocity.y * delta_time;
    bounds = (Rectangle) { position->x, position->y, size, size };
    if (collides_with_world(game, bounds)) {
        position->y = original.y;
    }
}

static void spawn_projectile(Game *game, Vector2 center, Vector2 direction,
                             float speed, Faction faction, int damage)
{
    for (int index = 0; index < MAX_PROJECTILES; ++index) {
        Projectile *projectile = &game->projectiles[index];
        if (!projectile->active) {
            projectile->position = (Vector2) {
                center.x - (float)PROJECTILE_SIZE / 2.0f,
                center.y - (float)PROJECTILE_SIZE / 2.0f,
            };
            projectile->velocity = (Vector2) { direction.x * speed, direction.y * speed };
            projectile->faction = faction;
            projectile->damage = damage;
            projectile->collision_layer = faction == FACTION_PLAYER
                                               ? COLLISION_PLAYER_SHOT
                                               : COLLISION_ENEMY_SHOT;
            projectile->collision_mask = faction == FACTION_PLAYER
                                              ? COLLISION_WORLD | COLLISION_ENEMY
                                              : COLLISION_WORLD | COLLISION_PLAYER;
            projectile->remaining_pierces =
                faction == FACTION_PLAYER ? game->inventory.pierce_bonus : 0;
            projectile->hit_enemy_mask = 0U;
            projectile->active = true;
            return;
        }
    }
}

static void fire_player_projectile(Game *game)
{
    Vector2 center = {
        game->player.position.x + (float)PLAYER_SIZE / 2.0f,
        game->player.position.y + (float)PLAYER_SIZE / 2.0f,
    };
    spawn_projectile(game, center, game->aim_direction, PLAYER_PROJECTILE_SPEED,
                     FACTION_PLAYER, 1 + game->inventory.damage_bonus);
}

EntityHandle game_spawn_enemy(Game *game, EnemyKind kind, Vector2 position)
{
    Rectangle spawn_bounds = { position.x, position.y, ENEMY_SIZE, ENEMY_SIZE };
    if ((kind != ENEMY_CHASER && kind != ENEMY_SPITTER) ||
        spawn_bounds.x < ROOM_INSET || spawn_bounds.y < ROOM_INSET ||
        spawn_bounds.x + spawn_bounds.width > LOGICAL_WIDTH - ROOM_INSET ||
        spawn_bounds.y + spawn_bounds.height > LOGICAL_HEIGHT - ROOM_INSET ||
        collides_with_world(game, spawn_bounds) ||
        rectangles_overlap(spawn_bounds, player_bounds(game))) {
        return (EntityHandle) { 0 };
    }

    for (int index = 0; index < MAX_ENEMIES; ++index) {
        if (game->enemies[index].active &&
            rectangles_overlap(spawn_bounds, enemy_bounds(&game->enemies[index]))) {
            return (EntityHandle) { 0 };
        }
    }

    for (uint16_t index = 0; index < MAX_ENEMIES; ++index) {
        Enemy *enemy = &game->enemies[index];
        if (!enemy->active) {
            ++enemy->generation;
            if (enemy->generation == 0) {
                ++enemy->generation;
            }
            enemy->position = position;
            enemy->kind = kind;
            enemy->health = kind == ENEMY_CHASER ? 3 : 2;
            enemy->attack_cooldown = kind == ENEMY_SPITTER ? 0.8f : 0.0f;
            enemy->collision_layer = COLLISION_ENEMY;
            enemy->collision_mask = COLLISION_WORLD | COLLISION_PLAYER |
                                    COLLISION_PLAYER_SHOT;
            enemy->active = true;
            return (EntityHandle) {
                ENTITY_ENEMY, index, enemy->generation, game->run_generation
            };
        }
    }
    return (EntityHandle) { 0 };
}

static PickupKind deterministic_item(const Game *game, int salt)
{
    uint32_t value = game->floor.seed ^
                     ((uint32_t)(game->floor.current_room + 1) * 0x9e3779b9U) ^
                     ((uint32_t)(salt + 1) * 0x85ebca6bU);
    value ^= value >> 16U;
    return (PickupKind)(PICKUP_DAMAGE_ITEM + value % 6U);
}

static void prepare_room_rewards(Game *game, Room *room)
{
    if (room->rewards_spawned) {
        return;
    }
    if (room->type != ROOM_REWARD && room->type != ROOM_SHOP &&
        room->type != ROOM_SECRET) {
        return;
    }
    room->rewards_spawned = true;

    if (room->type == ROOM_REWARD) {
        room->reward_kinds[0] = deterministic_item(game, 0);
        room->reward_kinds[1] = deterministic_item(game, 1);
        room->reward_available[0] = true;
        room->reward_available[1] = true;
    } else if (room->type == ROOM_SHOP) {
        room->reward_kinds[0] = deterministic_item(game, 2);
        room->reward_kinds[1] = deterministic_item(game, 3);
        room->reward_prices[0] = 5;
        room->reward_prices[1] = 7;
        room->reward_available[0] = true;
        room->reward_available[1] = true;
    } else if (room->type == ROOM_SECRET) {
        room->reward_kinds[0] = PICKUP_ACTIVE_ITEM;
        room->reward_available[0] = true;
    }
}

static void spawn_room_pickups(Game *game, const Room *room)
{
    static const Vector2 positions[2] = {
        { 430.0f, 250.0f }, { 506.0f, 250.0f }
    };
    for (int slot = 0; slot < 2; ++slot) {
        if (!room->reward_available[slot]) {
            continue;
        }
        game->pickups[slot] = (Pickup) {
            .position = positions[slot],
            .kind = (PickupKind)room->reward_kinds[slot],
            .price = room->reward_prices[slot],
            .room_slot = slot,
            .requires_key = room->type == ROOM_REWARD,
            .active = true,
        };
    }
}

static void create_clear_reward(Game *game, Room *room)
{
    if (room->rewards_spawned || room->type == ROOM_START) {
        return;
    }
    room->rewards_spawned = true;
    uint32_t value = game->floor.seed + (uint32_t)game->floor.current_room * 17U;
    room->reward_kinds[0] = (int)(value % 4U);
    room->reward_available[0] = true;
    spawn_room_pickups(game, room);
}

static void clear_room_entities(Game *game)
{
    for (int index = 0; index < MAX_ENEMIES; ++index) {
        game->enemies[index].active = false;
    }
    for (int index = 0; index < MAX_PROJECTILES; ++index) {
        game->projectiles[index].active = false;
    }
    for (int index = 0; index < MAX_PICKUPS; ++index) {
        game->pickups[index].active = false;
    }
    for (int index = 0; index < MAX_BOMBS; ++index) {
        game->placed_bombs[index].active = false;
    }
}

static void load_current_room(Game *game)
{
    clear_room_entities(game);
    Room *room = floor_current_room(&game->floor);
    room->visited = true;
    prepare_room_rewards(game, room);
    spawn_room_pickups(game, room);

    if (room->cleared) {
        room->doors_open = true;
        return;
    }

    room->doors_open = false;
    game_spawn_enemy(game, ENEMY_CHASER, (Vector2) { 150.0f, 110.0f });
    game_spawn_enemy(game, ENEMY_CHASER, (Vector2) { 774.0f, 110.0f });
    game_spawn_enemy(game, ENEMY_SPITTER, (Vector2) { 462.0f, 390.0f });
    if (room->type == ROOM_BOSS) {
        game_spawn_enemy(game, ENEMY_SPITTER, (Vector2) { 462.0f, 190.0f });
    }
}

bool game_enter_room(Game *game, int room_index)
{
    if (room_index < 0 || room_index >= game->floor.room_count) {
        return false;
    }
    game->floor.current_room = room_index;
    load_current_room(game);
    return true;
}

static bool initialize_game(Game *game, uint32_t run_generation, uint32_t seed)
{
    *game = (Game) { 0 };
    game->run_generation = run_generation == 0 ? 1 : run_generation;
    game->player = (Player) {
        .position = { 460.0f, 440.0f },
        .health = 6,
        .maximum_health = 6,
        .generation = 1,
    };
    game->inventory = (RunInventory) {
        .bombs = 2,
        .attack_rate_multiplier = 1.0f,
        .speed_multiplier = 1.0f,
        .active_charge_maximum = 3,
    };
    game->aim_direction = (Vector2) { 0.0f, -1.0f };
    if (!floor_generate(&game->floor, seed)) {
        return false;
    }
    load_current_room(game);
    return true;
}

void game_init(Game *game)
{
    (void)initialize_game(game, 1, 0xB10D2026U);
}

bool game_init_with_seed(Game *game, uint32_t seed)
{
    return initialize_game(game, 1, seed);
}

void game_restart(Game *game)
{
    uint32_t next_generation = game->run_generation + 1U;
    if (next_generation == 0U) {
        next_generation = 1U;
    }
    uint32_t seed = game->floor.seed;
    (void)initialize_game(game, next_generation, seed);
}

EntityHandle game_player_handle(const Game *game)
{
    return (EntityHandle) {
        ENTITY_PLAYER, 0, game->player.generation, game->run_generation
    };
}

bool game_handle_is_valid(const Game *game, EntityHandle handle)
{
    if (handle.run_generation != game->run_generation) {
        return false;
    }
    if (handle.kind == ENTITY_PLAYER) {
        return handle.index == 0 && handle.generation == game->player.generation &&
               game->player.health > 0;
    }
    if (handle.kind == ENTITY_ENEMY && handle.index < MAX_ENEMIES) {
        const Enemy *enemy = &game->enemies[handle.index];
        return enemy->active && enemy->generation == handle.generation;
    }
    return false;
}

bool game_apply_damage(Game *game, EntityHandle target, Faction source, int amount)
{
    if (amount <= 0 || !game_handle_is_valid(game, target)) {
        return false;
    }

    if (target.kind == ENTITY_PLAYER) {
        if (source == FACTION_PLAYER || game->player.invulnerability > 0.0f) {
            return false;
        }
        game->player.health -= amount;
        game->player.invulnerability = PLAYER_INVULNERABILITY;
        if (game->player.health <= 0) {
            game->player.health = 0;
            game->game_over = true;
        }
        return true;
    }

    if (source == FACTION_ENEMY) {
        return false;
    }
    Enemy *enemy = &game->enemies[target.index];
    enemy->health -= amount;
    if (enemy->health <= 0) {
        enemy->active = false;
    }
    return true;
}

static void place_bomb(Game *game)
{
    if (game->inventory.bombs <= 0) {
        return;
    }
    for (int index = 0; index < MAX_BOMBS; ++index) {
        Bomb *bomb = &game->placed_bombs[index];
        if (!bomb->active) {
            bomb->position = (Vector2) {
                game->player.position.x + (float)PLAYER_SIZE / 2.0f,
                game->player.position.y + (float)PLAYER_SIZE / 2.0f,
            };
            bomb->fuse = BOMB_FUSE;
            bomb->active = true;
            --game->inventory.bombs;
            return;
        }
    }
}

static void use_active_item(Game *game)
{
    if (!game->inventory.has_active_item ||
        game->inventory.active_charge < game->inventory.active_charge_maximum) {
        return;
    }
    game->inventory.active_charge = 0;
    for (uint16_t index = 0; index < MAX_ENEMIES; ++index) {
        Enemy *enemy = &game->enemies[index];
        if (enemy->active) {
            EntityHandle handle = {
                ENTITY_ENEMY, index, enemy->generation, game->run_generation
            };
            game_apply_damage(game, handle, FACTION_PLAYER, 2);
        }
    }
}

void game_handle_actions(Game *game, const GameInput *input)
{
    if (input->restart && game->game_over) {
        game_restart(game);
        return;
    }
    if (input->toggle_pause && !game->game_over) {
        game->paused = !game->paused;
    }
    if (!game->paused && !game->game_over && input->place_bomb) {
        place_bomb(game);
    }
    if (!game->paused && !game->game_over && input->use_active_item) {
        use_active_item(game);
    }
}

static void update_enemies(Game *game, float delta_time)
{
    for (uint16_t index = 0; index < MAX_ENEMIES; ++index) {
        Enemy *enemy = &game->enemies[index];
        if (!enemy->active) {
            continue;
        }

        Vector2 to_player = {
            game->player.position.x - enemy->position.x,
            game->player.position.y - enemy->position.y,
        };
        Vector2 direction = normalized(to_player);
        float speed = enemy->kind == ENEMY_CHASER ? CHASER_SPEED : SPITTER_SPEED;
        if (enemy->kind == ENEMY_SPITTER &&
            to_player.x * to_player.x + to_player.y * to_player.y < 90000.0f) {
            speed = -SPITTER_SPEED;
        }
        Vector2 velocity = { direction.x * speed, direction.y * speed };
        move_with_world_collision(game, &enemy->position, ENEMY_SIZE, velocity, delta_time);

        if (enemy->kind == ENEMY_SPITTER) {
            enemy->attack_cooldown -= delta_time;
            if (enemy->attack_cooldown <= 0.0f) {
                Vector2 center = {
                    enemy->position.x + (float)ENEMY_SIZE / 2.0f,
                    enemy->position.y + (float)ENEMY_SIZE / 2.0f,
                };
                spawn_projectile(game, center, direction, ENEMY_PROJECTILE_SPEED,
                                 FACTION_ENEMY, 1);
                enemy->attack_cooldown = SPITTER_SHOT_COOLDOWN;
            }
        }

        if ((enemy->collision_mask & COLLISION_PLAYER) != 0U &&
            rectangles_overlap(enemy_bounds(enemy), player_bounds(game))) {
            game_apply_damage(game, game_player_handle(game), FACTION_ENEMY, 1);
        }
    }
}

static void update_projectiles(Game *game, float delta_time)
{
    for (int projectile_index = 0; projectile_index < MAX_PROJECTILES; ++projectile_index) {
        Projectile *projectile = &game->projectiles[projectile_index];
        if (!projectile->active) {
            continue;
        }

        projectile->position.x += projectile->velocity.x * delta_time;
        projectile->position.y += projectile->velocity.y * delta_time;
        Rectangle bounds = projectile_bounds(projectile);
        if ((projectile->collision_mask & COLLISION_WORLD) != 0U &&
            collides_with_world(game, bounds)) {
            projectile->active = false;
            continue;
        }

        if ((projectile->collision_mask & COLLISION_PLAYER) != 0U) {
            if (rectangles_overlap(bounds, player_bounds(game))) {
                game_apply_damage(game, game_player_handle(game), FACTION_ENEMY,
                                  projectile->damage);
                projectile->active = false;
            }
            continue;
        }

        for (uint16_t enemy_index = 0; enemy_index < MAX_ENEMIES; ++enemy_index) {
            Enemy *enemy = &game->enemies[enemy_index];
            uint32_t enemy_bit = 1U << enemy_index;
            if (enemy->active && (projectile->hit_enemy_mask & enemy_bit) == 0U &&
                (projectile->collision_mask & enemy->collision_layer) != 0U &&
                (enemy->collision_mask & projectile->collision_layer) != 0U &&
                rectangles_overlap(bounds, enemy_bounds(enemy))) {
                EntityHandle handle = {
                    ENTITY_ENEMY, enemy_index, enemy->generation, game->run_generation
                };
                game_apply_damage(game, handle, FACTION_PLAYER, projectile->damage);
                projectile->hit_enemy_mask |= enemy_bit;
                if (projectile->remaining_pierces > 0) {
                    --projectile->remaining_pierces;
                } else {
                    projectile->active = false;
                }
                break;
            }
        }
    }
}

static bool direction_available(const Room *room, Direction direction)
{
    return (room->door_mask & (1U << (unsigned int)direction)) != 0U;
}

static void apply_pickup(Game *game, PickupKind kind)
{
    switch (kind) {
        case PICKUP_COIN: game->inventory.coins += 3; break;
        case PICKUP_KEY: ++game->inventory.keys; break;
        case PICKUP_BOMB: ++game->inventory.bombs; break;
        case PICKUP_HEALTH:
            if (game->player.health < game->player.maximum_health) {
                ++game->player.health;
            }
            break;
        case PICKUP_DAMAGE_ITEM: ++game->inventory.damage_bonus; break;
        case PICKUP_RATE_ITEM:
            game->inventory.attack_rate_multiplier *= 0.82f;
            if (game->inventory.attack_rate_multiplier < 0.35f) {
                game->inventory.attack_rate_multiplier = 0.35f;
            }
            break;
        case PICKUP_SPEED_ITEM:
            game->inventory.speed_multiplier += 0.12f;
            if (game->inventory.speed_multiplier > 1.8f) {
                game->inventory.speed_multiplier = 1.8f;
            }
            break;
        case PICKUP_PIERCE_ITEM: ++game->inventory.pierce_bonus; break;
        case PICKUP_HEALTH_ITEM:
            ++game->player.maximum_health;
            ++game->player.health;
            break;
        case PICKUP_ACTIVE_ITEM:
            game->inventory.has_active_item = true;
            game->inventory.active_charge = game->inventory.active_charge_maximum;
            break;
    }
}

static void update_pickups(Game *game, const GameInput *input)
{
    Rectangle player = player_bounds(game);
    Room *room = floor_current_room(&game->floor);
    for (int index = 0; index < MAX_PICKUPS; ++index) {
        Pickup *pickup = &game->pickups[index];
        if (!pickup->active) {
            continue;
        }
        Rectangle bounds = { pickup->position.x, pickup->position.y, 24.0f, 24.0f };
        if (!rectangles_overlap(player, bounds)) {
            continue;
        }
        if (pickup->kind == PICKUP_HEALTH &&
            game->player.health >= game->player.maximum_health) {
            continue;
        }
        if (pickup->price > 0) {
            if (!input->interact || game->inventory.coins < pickup->price) {
                continue;
            }
            game->inventory.coins -= pickup->price;
        }
        if (pickup->requires_key) {
            if (!input->interact || game->inventory.keys <= 0) {
                continue;
            }
            --game->inventory.keys;
        }

        apply_pickup(game, pickup->kind);
        pickup->active = false;
        if (pickup->room_slot >= 0 && pickup->room_slot < 2) {
            room->reward_available[pickup->room_slot] = false;
        }
        if (room->type == ROOM_REWARD && pickup->kind >= PICKUP_DAMAGE_ITEM) {
            room->reward_available[0] = false;
            room->reward_available[1] = false;
            for (int other = 0; other < MAX_PICKUPS; ++other) {
                game->pickups[other].active = false;
            }
        }
    }
}

static void reveal_secret_from_bomb(Game *game, Vector2 center)
{
    static const Vector2 door_centers[DIRECTION_COUNT] = {
        { LOGICAL_WIDTH / 2.0f, ROOM_INSET },
        { LOGICAL_WIDTH - ROOM_INSET, LOGICAL_HEIGHT / 2.0f },
        { LOGICAL_WIDTH / 2.0f, LOGICAL_HEIGHT - ROOM_INSET },
        { ROOM_INSET, LOGICAL_HEIGHT / 2.0f },
    };
    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
        int neighbor = floor_neighbor(&game->floor, game->floor.current_room,
                                      (Direction)direction);
        if (neighbor < 0 || game->floor.rooms[neighbor].type != ROOM_SECRET) {
            continue;
        }
        float dx = center.x - door_centers[direction].x;
        float dy = center.y - door_centers[direction].y;
        if (dx * dx + dy * dy <= BOMB_RADIUS * BOMB_RADIUS) {
            game->floor.rooms[neighbor].revealed = true;
        }
    }
}

static void update_bombs(Game *game, float delta_time)
{
    for (int index = 0; index < MAX_BOMBS; ++index) {
        Bomb *bomb = &game->placed_bombs[index];
        if (!bomb->active) {
            continue;
        }
        bomb->fuse -= delta_time;
        if (bomb->fuse > 0.0f) {
            continue;
        }
        bomb->active = false;
        reveal_secret_from_bomb(game, bomb->position);
        for (uint16_t enemy_index = 0; enemy_index < MAX_ENEMIES; ++enemy_index) {
            Enemy *enemy = &game->enemies[enemy_index];
            if (!enemy->active) {
                continue;
            }
            float dx = bomb->position.x - (enemy->position.x + ENEMY_SIZE / 2.0f);
            float dy = bomb->position.y - (enemy->position.y + ENEMY_SIZE / 2.0f);
            if (dx * dx + dy * dy <= BOMB_RADIUS * BOMB_RADIUS) {
                EntityHandle handle = {
                    ENTITY_ENEMY, enemy_index, enemy->generation, game->run_generation
                };
                game_apply_damage(game, handle, FACTION_PLAYER, 2);
            }
        }
    }
}

static bool try_room_transition(Game *game, Vector2 movement, float delta_time)
{
    Room *room = floor_current_room(&game->floor);
    if (!room->doors_open) {
        return false;
    }

    float next_x = game->player.position.x + movement.x * PLAYER_SPEED * delta_time;
    float next_y = game->player.position.y + movement.y * PLAYER_SPEED * delta_time;
    Direction direction = DIRECTION_COUNT;
    if (next_x < 0.0f) {
        direction = DIRECTION_LEFT;
    } else if (next_x + PLAYER_SIZE > LOGICAL_WIDTH) {
        direction = DIRECTION_RIGHT;
    } else if (next_y < 0.0f) {
        direction = DIRECTION_UP;
    } else if (next_y + PLAYER_SIZE > LOGICAL_HEIGHT) {
        direction = DIRECTION_DOWN;
    }

    if (direction == DIRECTION_COUNT || !direction_available(room, direction) ||
        !floor_connection_revealed(&game->floor, game->floor.current_room, direction)) {
        return false;
    }
    int neighbor = floor_neighbor(&game->floor, game->floor.current_room, direction);
    if (neighbor < 0) {
        return false;
    }

    game->floor.current_room = neighbor;
    if (direction == DIRECTION_LEFT) {
        game->player.position.x = LOGICAL_WIDTH - ROOM_INSET - PLAYER_SIZE;
    } else if (direction == DIRECTION_RIGHT) {
        game->player.position.x = ROOM_INSET;
    } else if (direction == DIRECTION_UP) {
        game->player.position.y = LOGICAL_HEIGHT - ROOM_INSET - PLAYER_SIZE;
    } else {
        game->player.position.y = ROOM_INSET;
    }
    load_current_room(game);
    return true;
}

void game_update(Game *game, const GameInput *input, float delta_time)
{
    if (game->paused || game->game_over) {
        return;
    }

    if (input->aim_direction.x != 0.0f || input->aim_direction.y != 0.0f) {
        game->aim_direction = input->aim_direction;
    }
    Vector2 player_velocity = {
        input->move_direction.x * PLAYER_SPEED * game->inventory.speed_multiplier,
        input->move_direction.y * PLAYER_SPEED * game->inventory.speed_multiplier,
    };
    if (!try_room_transition(game, input->move_direction, delta_time)) {
        move_with_world_collision(game, &game->player.position, PLAYER_SIZE,
                                  player_velocity, delta_time);
    }

    if (game->player.invulnerability > 0.0f) {
        game->player.invulnerability -= delta_time;
    }
    if (game->shot_cooldown > 0.0f) {
        game->shot_cooldown -= delta_time;
    }
    if (input->shooting && game->shot_cooldown <= 0.0f) {
        fire_player_projectile(game);
        game->shot_cooldown = PLAYER_SHOT_COOLDOWN *
                              game->inventory.attack_rate_multiplier;
    }

    update_enemies(game, delta_time);
    update_projectiles(game, delta_time);
    update_bombs(game, delta_time);
    update_pickups(game, input);

    if (game_active_enemy_count(game) == 0) {
        Room *room = floor_current_room(&game->floor);
        bool newly_cleared = !room->cleared;
        room->cleared = true;
        room->doors_open = true;
        if (newly_cleared) {
            create_clear_reward(game, room);
            if (game->inventory.has_active_item &&
                game->inventory.active_charge < game->inventory.active_charge_maximum) {
                ++game->inventory.active_charge;
            }
        }
    }
}

int game_active_projectile_count(const Game *game)
{
    int count = 0;
    for (int index = 0; index < MAX_PROJECTILES; ++index) {
        if (game->projectiles[index].active) {
            ++count;
        }
    }
    return count;
}

int game_active_enemy_count(const Game *game)
{
    int count = 0;
    for (int index = 0; index < MAX_ENEMIES; ++index) {
        if (game->enemies[index].active) {
            ++count;
        }
    }
    return count;
}

int game_active_pickup_count(const Game *game)
{
    int count = 0;
    for (int index = 0; index < MAX_PICKUPS; ++index) {
        if (game->pickups[index].active) {
            ++count;
        }
    }
    return count;
}
