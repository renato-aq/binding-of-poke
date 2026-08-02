#include "renderer.h"

#include "config.h"

static void draw_health(SDL_Renderer *renderer, const Game *game)
{
    for (int index = 0; index < game->player.maximum_health; ++index) {
        SDL_Rect segment = { 42 + index * 24, 42, 18, 14 };
        if (index < game->player.health) {
            SDL_SetRenderDrawColor(renderer, 238, 72, 72, 255);
            SDL_RenderFillRect(renderer, &segment);
        } else {
            SDL_SetRenderDrawColor(renderer, 76, 48, 54, 255);
            SDL_RenderDrawRect(renderer, &segment);
        }
    }
}

static void draw_door(SDL_Renderer *renderer, SDL_Rect door, bool open)
{
    if (open) {
        SDL_SetRenderDrawColor(renderer, 18, 20, 28, 255);
        SDL_RenderFillRect(renderer, &door);
        SDL_SetRenderDrawColor(renderer, 68, 190, 104, 255);
        SDL_RenderDrawRect(renderer, &door);
    } else {
        SDL_SetRenderDrawColor(renderer, 180, 62, 56, 255);
        SDL_RenderFillRect(renderer, &door);
    }
}

static void draw_doors(SDL_Renderer *renderer, const Room *room)
{
    SDL_Rect doors[DIRECTION_COUNT] = {
        { LOGICAL_WIDTH / 2 - 45, ROOM_INSET - 8, 90, 16 },
        { LOGICAL_WIDTH - ROOM_INSET - 8, LOGICAL_HEIGHT / 2 - 45, 16, 90 },
        { LOGICAL_WIDTH / 2 - 45, LOGICAL_HEIGHT - ROOM_INSET - 8, 90, 16 },
        { ROOM_INSET - 8, LOGICAL_HEIGHT / 2 - 45, 16, 90 },
    };
    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
        if ((room->door_mask & (1U << (unsigned int)direction)) != 0U) {
            draw_door(renderer, doors[direction], room->doors_open);
        }
    }
}

static void set_room_color(SDL_Renderer *renderer, RoomType type)
{
    switch (type) {
        case ROOM_START: SDL_SetRenderDrawColor(renderer, 110, 150, 220, 255); break;
        case ROOM_REWARD: SDL_SetRenderDrawColor(renderer, 230, 190, 52, 255); break;
        case ROOM_SHOP: SDL_SetRenderDrawColor(renderer, 72, 190, 130, 255); break;
        case ROOM_SECRET: SDL_SetRenderDrawColor(renderer, 170, 110, 210, 255); break;
        case ROOM_BOSS: SDL_SetRenderDrawColor(renderer, 210, 62, 62, 255); break;
        case ROOM_COMBAT: SDL_SetRenderDrawColor(renderer, 118, 124, 142, 255); break;
    }
}

static void draw_minimap(SDL_Renderer *renderer, const Floor *floor)
{
    const int cell_size = 11;
    const int map_center_x = LOGICAL_WIDTH - 84;
    const int map_center_y = 68;
    for (int index = 0; index < floor->room_count; ++index) {
        const Room *room = &floor->rooms[index];
        if (!room->visited) {
            continue;
        }
        set_room_color(renderer, room->type);
        SDL_Rect cell = {
            map_center_x + room->grid_x * (cell_size + 3),
            map_center_y + room->grid_y * (cell_size + 3),
            cell_size,
            cell_size,
        };
        if (index == floor->current_room) {
            SDL_RenderFillRect(renderer, &cell);
        } else {
            SDL_RenderDrawRect(renderer, &cell);
        }
    }
}

static void draw_overlay(SDL_Renderer *renderer, bool game_over)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 185);
    SDL_Rect overlay = { 0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT };
    SDL_RenderFillRect(renderer, &overlay);

    if (game_over) {
        SDL_SetRenderDrawColor(renderer, 225, 68, 68, 255);
        SDL_Rect horizontal = { LOGICAL_WIDTH / 2 - 60, LOGICAL_HEIGHT / 2 - 10, 120, 20 };
        SDL_Rect vertical = { LOGICAL_WIDTH / 2 - 10, LOGICAL_HEIGHT / 2 - 60, 20, 120 };
        SDL_RenderFillRect(renderer, &horizontal);
        SDL_RenderFillRect(renderer, &vertical);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 236, 80, 255);
        SDL_Rect left_bar = { LOGICAL_WIDTH / 2 - 34, LOGICAL_HEIGHT / 2 - 48, 22, 96 };
        SDL_Rect right_bar = { LOGICAL_WIDTH / 2 + 12, LOGICAL_HEIGHT / 2 - 48, 22, 96 };
        SDL_RenderFillRect(renderer, &left_bar);
        SDL_RenderFillRect(renderer, &right_bar);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void renderer_draw(SDL_Renderer *renderer, const Game *game)
{
    const Room *room = floor_current_room_const(&game->floor);
    SDL_SetRenderDrawColor(renderer, 18, 20, 28, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 58, 62, 76, 255);
    SDL_Rect room_border = {
        ROOM_INSET, ROOM_INSET,
        LOGICAL_WIDTH - ROOM_INSET * 2, LOGICAL_HEIGHT - ROOM_INSET * 2
    };
    SDL_RenderDrawRect(renderer, &room_border);
    draw_doors(renderer, room);

    SDL_SetRenderDrawColor(renderer, 76, 82, 98, 255);
    for (int index = 0; index < room->wall_count; ++index) {
        const Rectangle *wall = &room->walls[index];
        SDL_Rect wall_rect = {
            (int)wall->x, (int)wall->y, (int)wall->width, (int)wall->height
        };
        SDL_RenderFillRect(renderer, &wall_rect);
    }

    set_room_color(renderer, room->type);
    SDL_Rect room_type_marker = { LOGICAL_WIDTH / 2 - 18, 44, 36, 8 };
    SDL_RenderFillRect(renderer, &room_type_marker);

    for (int index = 0; index < MAX_ENEMIES; ++index) {
        const Enemy *enemy = &game->enemies[index];
        if (!enemy->active) {
            continue;
        }
        if (enemy->kind == ENEMY_CHASER) {
            SDL_SetRenderDrawColor(renderer, 220, 74, 74, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 174, 82, 218, 255);
        }
        SDL_Rect enemy_rect = {
            (int)enemy->position.x, (int)enemy->position.y, ENEMY_SIZE, ENEMY_SIZE
        };
        SDL_RenderFillRect(renderer, &enemy_rect);
    }

    bool player_visible = game->player.invulnerability <= 0.0f ||
                          ((int)(game->player.invulnerability * 16.0f) % 2 == 0);
    if (player_visible) {
        SDL_SetRenderDrawColor(renderer, 255, 236, 80, 255);
        SDL_Rect player = {
            (int)game->player.position.x, (int)game->player.position.y,
            PLAYER_SIZE, PLAYER_SIZE
        };
        SDL_RenderFillRect(renderer, &player);
    }

    for (int index = 0; index < MAX_PROJECTILES; ++index) {
        const Projectile *projectile = &game->projectiles[index];
        if (!projectile->active) {
            continue;
        }
        if (projectile->faction == FACTION_PLAYER) {
            SDL_SetRenderDrawColor(renderer, 255, 210, 40, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 228, 96, 220, 255);
        }
        SDL_Rect projectile_rect = {
            (int)projectile->position.x, (int)projectile->position.y,
            PROJECTILE_SIZE, PROJECTILE_SIZE
        };
        SDL_RenderFillRect(renderer, &projectile_rect);
    }

    draw_health(renderer, game);
    draw_minimap(renderer, &game->floor);
    if (game->paused || game->game_over) {
        draw_overlay(renderer, game->game_over);
    }
    SDL_RenderPresent(renderer);
}
