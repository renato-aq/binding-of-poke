#include "renderer.h"

#include <stdio.h>

#include "config.h"

static const char *pickup_description(PickupKind kind)
{
    switch (kind) {
        case PICKUP_COIN: return "Coin cache: +3 coins";
        case PICKUP_KEY: return "Key: opens one reward choice";
        case PICKUP_BOMB: return "Bomb: damage enemies and reveal secrets";
        case PICKUP_HEALTH: return "Berry: restore 1 HP";
        case PICKUP_DAMAGE_ITEM: return "Power Band: +1 projectile damage";
        case PICKUP_RATE_ITEM: return "Quick Claw: 18% faster attacks";
        case PICKUP_SPEED_ITEM: return "Swift Feather: +12% movement speed";
        case PICKUP_PIERCE_ITEM: return "Piercing Charge: +1 enemy pierced";
        case PICKUP_HEALTH_ITEM: return "Oran Pack: +1 maximum HP and heal 1";
        case PICKUP_ACTIVE_ITEM: return "Emergency Battery: shock room for 2 damage";
    }
    return "Unknown pickup";
}

static void update_window_title(SDL_Renderer *renderer, const Game *game)
{
    const Pickup *nearby = NULL;
    float player_x = game->player.position.x + PLAYER_SIZE / 2.0f;
    float player_y = game->player.position.y + PLAYER_SIZE / 2.0f;
    for (int index = 0; index < MAX_PICKUPS; ++index) {
        const Pickup *pickup = &game->pickups[index];
        if (!pickup->active) {
            continue;
        }
        float dx = player_x - (pickup->position.x + 12.0f);
        float dy = player_y - (pickup->position.y + 12.0f);
        if (dx * dx + dy * dy < 6400.0f) {
            nearby = pickup;
            break;
        }
    }

    char title[320];
    int rate_percent = (int)(game->inventory.attack_rate_multiplier * 100.0f);
    int speed_percent = (int)(game->inventory.speed_multiplier * 100.0f);
    if (nearby != NULL) {
        char price[32] = "";
        if (nearby->price > 0) {
            (void)snprintf(price, sizeof(price), " | Price: %d", nearby->price);
        } else if (nearby->requires_key) {
            (void)snprintf(price, sizeof(price), " | Requires 1 key");
        }
        (void)snprintf(title, sizeof(title),
                       "Seed %u | Wins:%u HP %d/%d C:%d K:%d B:%d DMG:+%d RATE:%d%% SPD:%d%% PIERCE:%d | %s%s",
                       game->floor.seed, game->completed_runs,
                       game->player.health, game->player.maximum_health,
                       game->inventory.coins, game->inventory.keys, game->inventory.bombs,
                       game->inventory.damage_bonus, rate_percent, speed_percent,
                       game->inventory.pierce_bonus, pickup_description(nearby->kind), price);
    } else {
        (void)snprintf(title, sizeof(title),
                       "Seed %u | Wins:%u HP %d/%d C:%d K:%d B:%d DMG:+%d RATE:%d%% SPD:%d%% PIERCE:%d",
                       game->floor.seed, game->completed_runs,
                       game->player.health, game->player.maximum_health,
                       game->inventory.coins, game->inventory.keys, game->inventory.bombs,
                       game->inventory.damage_bonus, rate_percent, speed_percent,
                       game->inventory.pierce_bonus);
    }
    SDL_SetWindowTitle(SDL_RenderGetWindow(renderer), title);
}

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

static void draw_doors(SDL_Renderer *renderer, const Floor *floor)
{
    const Room *room = floor_current_room_const(floor);
    SDL_Rect doors[DIRECTION_COUNT] = {
        { LOGICAL_WIDTH / 2 - 45, ROOM_INSET - 8, 90, 16 },
        { LOGICAL_WIDTH - ROOM_INSET - 8, LOGICAL_HEIGHT / 2 - 45, 16, 90 },
        { LOGICAL_WIDTH / 2 - 45, LOGICAL_HEIGHT - ROOM_INSET - 8, 90, 16 },
        { ROOM_INSET - 8, LOGICAL_HEIGHT / 2 - 45, 16, 90 },
    };
    for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
        if ((room->door_mask & (1U << (unsigned int)direction)) != 0U &&
            floor_connection_revealed(floor, floor->current_room, (Direction)direction)) {
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
        if (!room->visited && !(room->type == ROOM_SECRET && room->revealed)) {
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

static void draw_inventory(SDL_Renderer *renderer, const Game *game)
{
    const int values[3] = {
        game->inventory.coins, game->inventory.keys, game->inventory.bombs
    };
    const SDL_Color colors[3] = {
        { 236, 196, 62, 255 }, { 130, 190, 235, 255 }, { 115, 120, 132, 255 }
    };
    for (int group = 0; group < 3; ++group) {
        SDL_SetRenderDrawColor(renderer, colors[group].r, colors[group].g,
                               colors[group].b, colors[group].a);
        int shown = values[group] > 10 ? 10 : values[group];
        for (int index = 0; index < shown; ++index) {
            SDL_Rect tally = { 42 + index * 8, 70 + group * 14, 6, 8 };
            SDL_RenderFillRect(renderer, &tally);
        }
    }

    if (game->inventory.has_active_item) {
        for (int charge = 0; charge < game->inventory.active_charge_maximum; ++charge) {
            if (charge < game->inventory.active_charge) {
                SDL_SetRenderDrawColor(renderer, 248, 220, 74, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 70, 72, 82, 255);
            }
            SDL_Rect segment = { 42 + charge * 18, 116, 14, 8 };
            SDL_RenderFillRect(renderer, &segment);
        }
    }
}

static void draw_overlay(SDL_Renderer *renderer, bool game_over, bool victory)
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
    } else if (victory) {
        SDL_SetRenderDrawColor(renderer, 72, 220, 126, 255);
        SDL_Rect left = { LOGICAL_WIDTH / 2 - 58, LOGICAL_HEIGHT / 2, 52, 18 };
        SDL_Rect right = { LOGICAL_WIDTH / 2 - 15, LOGICAL_HEIGHT / 2 - 32, 90, 18 };
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
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
    update_window_title(renderer, game);
    const Room *room = floor_current_room_const(&game->floor);
    SDL_SetRenderDrawColor(renderer, 18, 20, 28, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 58, 62, 76, 255);
    SDL_Rect room_border = {
        ROOM_INSET, ROOM_INSET,
        LOGICAL_WIDTH - ROOM_INSET * 2, LOGICAL_HEIGHT - ROOM_INSET * 2
    };
    SDL_RenderDrawRect(renderer, &room_border);
    draw_doors(renderer, &game->floor);

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

    if (game->boss.active) {
        if (game->boss.state == BOSS_CHARGE_TELEGRAPH) {
            SDL_SetRenderDrawColor(renderer, 255, 196, 56, 255);
        } else if (game->boss.health <= game->boss.maximum_health / 2) {
            SDL_SetRenderDrawColor(renderer, 232, 62, 82, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 194, 72, 210, 255);
        }
        SDL_Rect boss_rect = {
            (int)game->boss.position.x, (int)game->boss.position.y,
            BOSS_SIZE, BOSS_SIZE
        };
        SDL_RenderFillRect(renderer, &boss_rect);

        SDL_SetRenderDrawColor(renderer, 58, 34, 42, 255);
        SDL_Rect bar_background = { 280, LOGICAL_HEIGHT - 24, 400, 10 };
        SDL_RenderFillRect(renderer, &bar_background);
        SDL_SetRenderDrawColor(renderer, 220, 58, 78, 255);
        SDL_Rect bar = {
            280, LOGICAL_HEIGHT - 24,
            400 * game->boss.health / game->boss.maximum_health, 10
        };
        SDL_RenderFillRect(renderer, &bar);
    }

    bool player_visible = game->reduced_flashes || game->player.invulnerability <= 0.0f ||
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

    for (int index = 0; index < MAX_PICKUPS; ++index) {
        const Pickup *pickup = &game->pickups[index];
        if (!pickup->active) {
            continue;
        }
        if (pickup->kind == PICKUP_COIN) {
            SDL_SetRenderDrawColor(renderer, 236, 196, 62, 255);
        } else if (pickup->kind == PICKUP_KEY) {
            SDL_SetRenderDrawColor(renderer, 130, 190, 235, 255);
        } else if (pickup->kind == PICKUP_BOMB) {
            SDL_SetRenderDrawColor(renderer, 115, 120, 132, 255);
        } else if (pickup->kind == PICKUP_HEALTH) {
            SDL_SetRenderDrawColor(renderer, 238, 72, 72, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 220, 150, 255);
        }
        SDL_Rect pickup_rect = {
            (int)pickup->position.x, (int)pickup->position.y, 24, 24
        };
        SDL_RenderFillRect(renderer, &pickup_rect);
        if (pickup->price > 0) {
            SDL_SetRenderDrawColor(renderer, 236, 196, 62, 255);
            SDL_Rect price_marker = { pickup_rect.x + 7, pickup_rect.y + 27, 10, 5 };
            SDL_RenderFillRect(renderer, &price_marker);
        }
    }

    SDL_SetRenderDrawColor(renderer, 72, 72, 82, 255);
    for (int index = 0; index < MAX_BOMBS; ++index) {
        const Bomb *bomb = &game->placed_bombs[index];
        if (bomb->active) {
            SDL_Rect bomb_rect = {
                (int)bomb->position.x - 9, (int)bomb->position.y - 9, 18, 18
            };
            SDL_RenderFillRect(renderer, &bomb_rect);
        }
    }

    draw_health(renderer, game);
    draw_inventory(renderer, game);
    draw_minimap(renderer, &game->floor);
    if (game->paused || game->game_over || game->victory) {
        draw_overlay(renderer, game->game_over, game->victory);
    }
    SDL_RenderPresent(renderer);
}
