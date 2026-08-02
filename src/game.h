#ifndef BIND_OF_POKE_GAME_H
#define BIND_OF_POKE_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "floor.h"

typedef struct Vector2 {
    float x;
    float y;
} Vector2;

typedef enum Faction {
    FACTION_PLAYER,
    FACTION_ENEMY,
} Faction;

typedef enum EntityKind {
    ENTITY_NONE,
    ENTITY_PLAYER,
    ENTITY_ENEMY,
} EntityKind;

typedef enum EnemyKind {
    ENEMY_CHASER,
    ENEMY_SPITTER,
} EnemyKind;

typedef enum CollisionLayer {
    COLLISION_NONE = 0,
    COLLISION_WORLD = 1 << 0,
    COLLISION_PLAYER = 1 << 1,
    COLLISION_ENEMY = 1 << 2,
    COLLISION_PLAYER_SHOT = 1 << 3,
    COLLISION_ENEMY_SHOT = 1 << 4,
} CollisionLayer;

typedef struct EntityHandle {
    EntityKind kind;
    uint16_t index;
    uint16_t generation;
    uint32_t run_generation;
} EntityHandle;

typedef struct GameInput {
    Vector2 move_direction;
    Vector2 aim_direction;
    bool shooting;
    bool toggle_pause;
    bool restart;
} GameInput;

typedef struct Player {
    Vector2 position;
    int health;
    int maximum_health;
    float invulnerability;
    uint16_t generation;
} Player;

typedef struct Enemy {
    Vector2 position;
    EnemyKind kind;
    int health;
    float attack_cooldown;
    uint32_t collision_layer;
    uint32_t collision_mask;
    uint16_t generation;
    bool active;
} Enemy;

typedef struct Projectile {
    Vector2 position;
    Vector2 velocity;
    Faction faction;
    int damage;
    uint32_t collision_layer;
    uint32_t collision_mask;
    bool active;
} Projectile;

typedef struct Game {
    Player player;
    Vector2 aim_direction;
    Enemy enemies[MAX_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    Floor floor;
    float shot_cooldown;
    uint32_t run_generation;
    bool paused;
    bool game_over;
} Game;

void game_init(Game *game);
bool game_init_with_seed(Game *game, uint32_t seed);
void game_restart(Game *game);
bool game_enter_room(Game *game, int room_index);
void game_handle_actions(Game *game, const GameInput *input);
void game_update(Game *game, const GameInput *input, float delta_time);
EntityHandle game_player_handle(const Game *game);
EntityHandle game_spawn_enemy(Game *game, EnemyKind kind, Vector2 position);
bool game_handle_is_valid(const Game *game, EntityHandle handle);
bool game_apply_damage(Game *game, EntityHandle target, Faction source, int amount);
int game_active_projectile_count(const Game *game);
int game_active_enemy_count(const Game *game);

#endif
