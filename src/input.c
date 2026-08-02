#include "input.h"

#include <SDL.h>

static Vector2 normalize_cardinal_direction(Vector2 direction)
{
    if (direction.x != 0.0f && direction.y != 0.0f) {
        direction.x *= 0.70710678f;
        direction.y *= 0.70710678f;
    }
    return direction;
}

void input_poll(AppInput *input, const Platform *platform, const Game *game)
{
    *input = (AppInput) { 0 };
    SDL_Event event;

    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            input->quit_requested = true;
        }
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                input->quit_requested = true;
            }
            if (event.key.keysym.sym == SDLK_p) {
                input->game.toggle_pause = true;
            }
        }
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    input->game.move_direction = normalize_cardinal_direction((Vector2) {
        .x = (float)(keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) -
             (float)(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]),
        .y = (float)(keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) -
             (float)(keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]),
    });

    Vector2 keyboard_aim = {
        .x = (float)keys[SDL_SCANCODE_L] - (float)keys[SDL_SCANCODE_J],
        .y = (float)keys[SDL_SCANCODE_K] - (float)keys[SDL_SCANCODE_I],
    };
    if (keyboard_aim.x != 0.0f || keyboard_aim.y != 0.0f) {
        input->game.aim_direction = normalize_cardinal_direction(keyboard_aim);
        input->game.shooting = true;
        return;
    }

    int mouse_x = 0;
    int mouse_y = 0;
    Uint32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    input->game.shooting = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0U;

    if (input->game.shooting) {
        float logical_x = 0.0f;
        float logical_y = 0.0f;
        platform_window_to_logical(platform, mouse_x, mouse_y, &logical_x, &logical_y);
        Vector2 aim = {
            .x = logical_x - (game->player_position.x + (float)PLAYER_SIZE / 2.0f),
            .y = logical_y - (game->player_position.y + (float)PLAYER_SIZE / 2.0f),
        };
        float length_squared = aim.x * aim.x + aim.y * aim.y;

        if (length_squared > 0.0f) {
            float inverse_length = 1.0f / SDL_sqrtf(length_squared);
            input->game.aim_direction.x = aim.x * inverse_length;
            input->game.aim_direction.y = aim.y * inverse_length;
        } else {
            input->game.shooting = false;
        }
    }
}
