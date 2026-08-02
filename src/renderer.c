#include "renderer.h"

#include "config.h"

void renderer_draw(SDL_Renderer *renderer, const Game *game)
{
    SDL_SetRenderDrawColor(renderer, 18, 20, 28, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 58, 62, 76, 255);
    SDL_Rect room_border = { 4, 4, LOGICAL_WIDTH - 8, LOGICAL_HEIGHT - 8 };
    SDL_RenderDrawRect(renderer, &room_border);

    SDL_SetRenderDrawColor(renderer, 255, 236, 80, 255);
    SDL_Rect player = {
        .x = (int)game->player_position.x,
        .y = (int)game->player_position.y,
        .w = PLAYER_SIZE,
        .h = PLAYER_SIZE,
    };
    SDL_RenderFillRect(renderer, &player);

    SDL_SetRenderDrawColor(renderer, 255, 210, 40, 255);
    for (int index = 0; index < MAX_PROJECTILES; ++index) {
        const Projectile *projectile = &game->projectiles[index];

        if (projectile->active) {
            SDL_Rect projectile_rect = {
                .x = (int)projectile->position.x,
                .y = (int)projectile->position.y,
                .w = PROJECTILE_SIZE,
                .h = PROJECTILE_SIZE,
            };
            SDL_RenderFillRect(renderer, &projectile_rect);
        }
    }

    if (game->paused) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 8, 10, 16, 180);
        SDL_Rect overlay = { 0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT };
        SDL_RenderFillRect(renderer, &overlay);

        SDL_SetRenderDrawColor(renderer, 255, 236, 80, 255);
        SDL_Rect left_bar = { LOGICAL_WIDTH / 2 - 34, LOGICAL_HEIGHT / 2 - 48, 22, 96 };
        SDL_Rect right_bar = { LOGICAL_WIDTH / 2 + 12, LOGICAL_HEIGHT / 2 - 48, 22, 96 };
        SDL_RenderFillRect(renderer, &left_bar);
        SDL_RenderFillRect(renderer, &right_bar);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    SDL_RenderPresent(renderer);
}
