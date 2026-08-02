#ifndef BIND_OF_POKE_GAME_H
#define BIND_OF_POKE_GAME_H

#include <stdbool.h>

#include "config.h"

typedef struct Vector2 {
    float x;
    float y;
} Vector2;

typedef struct GameInput {
    Vector2 move_direction;
    Vector2 aim_direction;
    bool shooting;
    bool toggle_pause;
} GameInput;

typedef struct Projectile {
    Vector2 position;
    Vector2 velocity;
    bool active;
} Projectile;

typedef struct Game {
    Vector2 player_position;
    Vector2 aim_direction;
    Projectile projectiles[MAX_PROJECTILES];
    float shot_cooldown;
    bool paused;
} Game;

void game_init(Game *game);
void game_handle_actions(Game *game, const GameInput *input);
void game_update(Game *game, const GameInput *input, float delta_time);
int game_active_projectile_count(const Game *game);

#endif
