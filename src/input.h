#ifndef BIND_OF_POKE_INPUT_H
#define BIND_OF_POKE_INPUT_H

#include <stdbool.h>

#include "game.h"
#include "platform.h"

typedef struct AppInput {
    GameInput game;
    bool quit_requested;
} AppInput;

void input_poll(AppInput *input, const Platform *platform, const Game *game);

#endif
