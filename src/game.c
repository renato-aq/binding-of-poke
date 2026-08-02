#include "game.h"

static const float PLAYER_SPEED = 260.0f;
static const float PROJECTILE_SPEED = 520.0f;
static const float SHOT_COOLDOWN = 0.16f;

static void fire_projectile(Game *game)
{
    for (int index = 0; index < MAX_PROJECTILES; ++index) {
        Projectile *projectile = &game->projectiles[index];

        if (!projectile->active) {
            projectile->position.x =
                game->player_position.x + (float)(PLAYER_SIZE - PROJECTILE_SIZE) / 2.0f;
            projectile->position.y =
                game->player_position.y + (float)(PLAYER_SIZE - PROJECTILE_SIZE) / 2.0f;
            projectile->velocity.x = game->aim_direction.x * PROJECTILE_SPEED;
            projectile->velocity.y = game->aim_direction.y * PROJECTILE_SPEED;
            projectile->active = true;
            return;
        }
    }
}

void game_init(Game *game)
{
    *game = (Game) {
        .player_position = {
            .x = (float)(LOGICAL_WIDTH - PLAYER_SIZE) / 2.0f,
            .y = (float)(LOGICAL_HEIGHT - PLAYER_SIZE) / 2.0f,
        },
        .aim_direction = { .x = 1.0f, .y = 0.0f },
    };
}

void game_handle_actions(Game *game, const GameInput *input)
{
    if (input->toggle_pause) {
        game->paused = !game->paused;
    }
}

void game_update(Game *game, const GameInput *input, float delta_time)
{
    if (game->paused) {
        return;
    }

    if (input->aim_direction.x != 0.0f || input->aim_direction.y != 0.0f) {
        game->aim_direction = input->aim_direction;
    }

    game->player_position.x += input->move_direction.x * PLAYER_SPEED * delta_time;
    game->player_position.y += input->move_direction.y * PLAYER_SPEED * delta_time;

    if (game->player_position.x < 0.0f) {
        game->player_position.x = 0.0f;
    }
    if (game->player_position.y < 0.0f) {
        game->player_position.y = 0.0f;
    }
    if (game->player_position.x > (float)(LOGICAL_WIDTH - PLAYER_SIZE)) {
        game->player_position.x = (float)(LOGICAL_WIDTH - PLAYER_SIZE);
    }
    if (game->player_position.y > (float)(LOGICAL_HEIGHT - PLAYER_SIZE)) {
        game->player_position.y = (float)(LOGICAL_HEIGHT - PLAYER_SIZE);
    }

    if (game->shot_cooldown > 0.0f) {
        game->shot_cooldown -= delta_time;
    }

    if (input->shooting && game->shot_cooldown <= 0.0f) {
        fire_projectile(game);
        game->shot_cooldown = SHOT_COOLDOWN;
    }

    for (int index = 0; index < MAX_PROJECTILES; ++index) {
        Projectile *projectile = &game->projectiles[index];

        if (!projectile->active) {
            continue;
        }

        projectile->position.x += projectile->velocity.x * delta_time;
        projectile->position.y += projectile->velocity.y * delta_time;

        if (projectile->position.x + PROJECTILE_SIZE < 0.0f ||
            projectile->position.y + PROJECTILE_SIZE < 0.0f ||
            projectile->position.x > (float)LOGICAL_WIDTH ||
            projectile->position.y > (float)LOGICAL_HEIGHT) {
            projectile->active = false;
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
