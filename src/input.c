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

static void close_controller(InputSystem *system)
{
    if (system->controller != NULL) {
        SDL_GameControllerClose(system->controller);
        system->controller = NULL;
        system->controller_id = -1;
        system->bomb_was_down = false;
        system->active_was_down = false;
    }
}

static void open_first_controller(InputSystem *system)
{
    if (system->controller != NULL) {
        return;
    }
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (!SDL_IsGameController(index)) {
            continue;
        }
        system->controller = SDL_GameControllerOpen(index);
        if (system->controller != NULL) {
            SDL_Joystick *joystick = SDL_GameControllerGetJoystick(system->controller);
            system->controller_id = SDL_JoystickInstanceID(joystick);
            return;
        }
    }
}

void input_init(InputSystem *system)
{
    *system = (InputSystem) { .controller_id = -1, .dead_zone = 8000 };
    open_first_controller(system);
}

void input_shutdown(InputSystem *system)
{
    close_controller(system);
}

static float axis_value(Sint16 value, int dead_zone)
{
    int magnitude = value < 0 ? -(int)value : (int)value;
    if (magnitude <= dead_zone) {
        return 0.0f;
    }
    float normalized = (float)(magnitude - dead_zone) / (32767.0f - (float)dead_zone);
    if (normalized > 1.0f) {
        normalized = 1.0f;
    }
    return value < 0 ? -normalized : normalized;
}

static void poll_controller(InputSystem *system, AppInput *input)
{
    if (system->controller == NULL) {
        return;
    }
    Vector2 movement = {
        axis_value(SDL_GameControllerGetAxis(system->controller,
                                             SDL_CONTROLLER_AXIS_LEFTX), system->dead_zone),
        axis_value(SDL_GameControllerGetAxis(system->controller,
                                             SDL_CONTROLLER_AXIS_LEFTY), system->dead_zone),
    };
    if (movement.x != 0.0f || movement.y != 0.0f) {
        input->game.move_direction = movement;
    }

    Vector2 aim = {
        axis_value(SDL_GameControllerGetAxis(system->controller,
                                             SDL_CONTROLLER_AXIS_RIGHTX), system->dead_zone),
        axis_value(SDL_GameControllerGetAxis(system->controller,
                                             SDL_CONTROLLER_AXIS_RIGHTY), system->dead_zone),
    };
    if (aim.x != 0.0f || aim.y != 0.0f) {
        float length_squared = aim.x * aim.x + aim.y * aim.y;
        float inverse_length = 1.0f / SDL_sqrtf(length_squared);
        input->game.aim_direction = (Vector2) {
            aim.x * inverse_length, aim.y * inverse_length
        };
        input->game.shooting = true;
    }

    input->game.interact |= SDL_GameControllerGetButton(
        system->controller, SDL_CONTROLLER_BUTTON_A) != 0;
    bool bomb_down = SDL_GameControllerGetButton(
        system->controller, SDL_CONTROLLER_BUTTON_X) != 0;
    input->game.place_bomb |= bomb_down && !system->bomb_was_down;
    system->bomb_was_down = bomb_down;
    bool active_down = SDL_GameControllerGetButton(
        system->controller, SDL_CONTROLLER_BUTTON_Y) != 0;
    input->game.use_active_item |= active_down && !system->active_was_down;
    system->active_was_down = active_down;
}

void input_poll(InputSystem *system, AppInput *input,
                const Platform *platform, const Game *game)
{
    *input = (AppInput) { 0 };
    SDL_Event event;

    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            input->quit_requested = true;
        } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            open_first_controller(system);
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED &&
                   event.cdevice.which == system->controller_id) {
            close_controller(system);
            open_first_controller(system);
        }
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_ESCAPE) input->quit_requested = true;
            if (event.key.keysym.sym == SDLK_p) input->game.toggle_pause = true;
            if (event.key.keysym.sym == SDLK_r) input->game.restart = true;
            if (event.key.keysym.sym == SDLK_e) input->game.interact = true;
            if (event.key.keysym.sym == SDLK_b) input->game.place_bomb = true;
            if (event.key.keysym.sym == SDLK_SPACE) input->game.use_active_item = true;
            if (event.key.keysym.sym == SDLK_f) input->game.toggle_reduced_flashes = true;
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                input->game.toggle_pause = true;
            }
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                input->game.restart = true;
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
    } else {
        int mouse_x = 0;
        int mouse_y = 0;
        Uint32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        input->game.shooting = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0U;
        if (input->game.shooting) {
            float logical_x = 0.0f;
            float logical_y = 0.0f;
            platform_window_to_logical(platform, mouse_x, mouse_y, &logical_x, &logical_y);
            Vector2 aim = {
                logical_x - (game->player.position.x + (float)PLAYER_SIZE / 2.0f),
                logical_y - (game->player.position.y + (float)PLAYER_SIZE / 2.0f),
            };
            float length_squared = aim.x * aim.x + aim.y * aim.y;
            if (length_squared > 0.0f) {
                float inverse_length = 1.0f / SDL_sqrtf(length_squared);
                input->game.aim_direction = (Vector2) {
                    aim.x * inverse_length, aim.y * inverse_length
                };
            } else {
                input->game.shooting = false;
            }
        }
    }
    poll_controller(system, input);
}
