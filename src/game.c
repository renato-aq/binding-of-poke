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
    if (bounds.x < ROOM_INSET || bounds.y < ROOM_INSET ||
        bounds.x + bounds.width > LOGICAL_WIDTH - ROOM_INSET ||
        bounds.y + bounds.height > LOGICAL_HEIGHT - ROOM_INSET) {
        return true;
    }

    for (int index = 0; index < game->room.wall_count; ++index) {
        if (rectangles_overlap(bounds, game->room.walls[index])) {
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
                     FACTION_PLAYER, 1);
}

static void initialize_room(Game *game)
{
    game->room = (Room) {
        .walls = {
            { 260.0f, 190.0f, 70.0f, 160.0f },
            { 630.0f, 190.0f, 70.0f, 160.0f },
            { 430.0f, 105.0f, 100.0f, 45.0f },
        },
        .wall_count = 3,
    };
}

EntityHandle game_spawn_enemy(Game *game, EnemyKind kind, Vector2 position)
{
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
            return (EntityHandle) { ENTITY_ENEMY, index, enemy->generation };
        }
    }
    return (EntityHandle) { 0 };
}

void game_init(Game *game)
{
    *game = (Game) { 0 };
    game->player = (Player) {
        .position = { 460.0f, 440.0f },
        .health = 6,
        .maximum_health = 6,
        .generation = 1,
    };
    game->aim_direction = (Vector2) { 0.0f, -1.0f };
    initialize_room(game);
    game_spawn_enemy(game, ENEMY_CHASER, (Vector2) { 170.0f, 120.0f });
    game_spawn_enemy(game, ENEMY_CHASER, (Vector2) { 750.0f, 120.0f });
    game_spawn_enemy(game, ENEMY_SPITTER, (Vector2) { 462.0f, 210.0f });
}

EntityHandle game_player_handle(const Game *game)
{
    return (EntityHandle) { ENTITY_PLAYER, 0, game->player.generation };
}

bool game_handle_is_valid(const Game *game, EntityHandle handle)
{
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

void game_handle_actions(Game *game, const GameInput *input)
{
    if (input->restart && game->game_over) {
        game_init(game);
        return;
    }
    if (input->toggle_pause && !game->game_over) {
        game->paused = !game->paused;
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
            if (enemy->active &&
                (projectile->collision_mask & enemy->collision_layer) != 0U &&
                (enemy->collision_mask & projectile->collision_layer) != 0U &&
                rectangles_overlap(bounds, enemy_bounds(enemy))) {
                EntityHandle handle = { ENTITY_ENEMY, enemy_index, enemy->generation };
                game_apply_damage(game, handle, FACTION_PLAYER, projectile->damage);
                projectile->active = false;
                break;
            }
        }
    }
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
        input->move_direction.x * PLAYER_SPEED,
        input->move_direction.y * PLAYER_SPEED,
    };
    move_with_world_collision(game, &game->player.position, PLAYER_SIZE,
                              player_velocity, delta_time);

    if (game->player.invulnerability > 0.0f) {
        game->player.invulnerability -= delta_time;
    }
    if (game->shot_cooldown > 0.0f) {
        game->shot_cooldown -= delta_time;
    }
    if (input->shooting && game->shot_cooldown <= 0.0f) {
        fire_player_projectile(game);
        game->shot_cooldown = PLAYER_SHOT_COOLDOWN;
    }

    update_enemies(game, delta_time);
    update_projectiles(game, delta_time);

    if (game_active_enemy_count(game) == 0) {
        game->room.cleared = true;
        game->room.doors_open = true;
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
