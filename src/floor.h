#ifndef BIND_OF_POKE_FLOOR_H
#define BIND_OF_POKE_FLOOR_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;

typedef enum Direction {
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_COUNT,
} Direction;

enum {
    DOOR_UP = 1 << DIRECTION_UP,
    DOOR_RIGHT = 1 << DIRECTION_RIGHT,
    DOOR_DOWN = 1 << DIRECTION_DOWN,
    DOOR_LEFT = 1 << DIRECTION_LEFT,
};

typedef enum RoomType {
    ROOM_START,
    ROOM_COMBAT,
    ROOM_REWARD,
    ROOM_SHOP,
    ROOM_SECRET,
    ROOM_BOSS,
} RoomType;

typedef struct Room {
    int grid_x;
    int grid_y;
    RoomType type;
    uint8_t door_mask;
    uint8_t template_index;
    Rectangle walls[MAX_WALLS];
    int wall_count;
    bool doors_open;
    bool cleared;
    bool visited;
    bool revealed;
    bool rewards_spawned;
    int reward_kinds[2];
    int reward_prices[2];
    bool reward_available[2];
} Room;

typedef struct Floor {
    Room rooms[MAX_FLOOR_ROOMS];
    int room_count;
    int current_room;
    uint32_t seed;
} Floor;

bool floor_generate(Floor *floor, uint32_t seed);
bool floor_validate(const Floor *floor);
int floor_find_room(const Floor *floor, int grid_x, int grid_y);
int floor_neighbor(const Floor *floor, int room_index, Direction direction);
Room *floor_current_room(Floor *floor);
const Room *floor_current_room_const(const Floor *floor);
bool floor_connection_revealed(const Floor *floor, int room_index, Direction direction);

#endif
