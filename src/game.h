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
    bool interact;
    bool place_bomb;
    bool use_active_item;
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
    int remaining_pierces;
    uint32_t hit_enemy_mask;
    bool active;
} Projectile;

typedef enum PickupKind {
    PICKUP_COIN,
    PICKUP_KEY,
    PICKUP_BOMB,
    PICKUP_HEALTH,
    PICKUP_DAMAGE_ITEM,
    PICKUP_RATE_ITEM,
    PICKUP_SPEED_ITEM,
    PICKUP_PIERCE_ITEM,
    PICKUP_HEALTH_ITEM,
    PICKUP_ACTIVE_ITEM,
} PickupKind;

typedef struct Pickup {
    Vector2 position;
    PickupKind kind;
    int price;
    int room_slot;
    bool requires_key;
    bool active;
} Pickup;

typedef struct Bomb {
    Vector2 position;
    float fuse;
    bool active;
} Bomb;

typedef struct RunInventory {
    int coins;
    int keys;
    int bombs;
    int damage_bonus;
    int pierce_bonus;
    int active_charge;
    int active_charge_maximum;
    float attack_rate_multiplier;
    float speed_multiplier;
    bool has_active_item;
} RunInventory;

typedef struct Game {
    Player player;
    Vector2 aim_direction;
    Enemy enemies[MAX_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    Pickup pickups[MAX_PICKUPS];
    Bomb placed_bombs[MAX_BOMBS];
    RunInventory inventory;
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
int game_active_pickup_count(const Game *game);

#endif
