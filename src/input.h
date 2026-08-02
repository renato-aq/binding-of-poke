#ifndef BIND_OF_POKE_INPUT_H
#define BIND_OF_POKE_INPUT_H

#include <stdbool.h>

#include <SDL.h>

#include "game.h"
#include "platform.h"

typedef struct AppInput {
    GameInput game;
    bool quit_requested;
} AppInput;

typedef struct InputSystem {
    SDL_GameController *controller;
    SDL_JoystickID controller_id;
    int dead_zone;
    bool bomb_was_down;
    bool active_was_down;
} InputSystem;

void input_init(InputSystem *system);
void input_shutdown(InputSystem *system);
void input_poll(InputSystem *system, AppInput *input,
                const Platform *platform, const Game *game);

#endif
