#include "floor.h"

#include <stdlib.h>

#include "rng.h"

static const int DIRECTION_X[DIRECTION_COUNT] = { 0, 1, 0, -1 };
static const int DIRECTION_Y[DIRECTION_COUNT] = { -1, 0, 1, 0 };

static void apply_template(Room *room)
{
    room->wall_count = 0;
    if (room->template_index == 0U) {
        room->walls[0] = (Rectangle) { 260.0f, 190.0f, 70.0f, 160.0f };
        room->walls[1] = (Rectangle) { 630.0f, 190.0f, 70.0f, 160.0f };
        room->walls[2] = (Rectangle) { 430.0f, 105.0f, 100.0f, 45.0f };
        room->wall_count = 3;
    } else if (room->template_index == 1U) {
        room->walls[0] = (Rectangle) { 210.0f, 245.0f, 150.0f, 50.0f };
        room->walls[1] = (Rectangle) { 600.0f, 245.0f, 150.0f, 50.0f };
        room->wall_count = 2;
    } else {
        room->walls[0] = (Rectangle) { 350.0f, 170.0f, 60.0f, 60.0f };
        room->walls[1] = (Rectangle) { 550.0f, 170.0f, 60.0f, 60.0f };
        room->walls[2] = (Rectangle) { 350.0f, 310.0f, 60.0f, 60.0f };
        room->walls[3] = (Rectangle) { 550.0f, 310.0f, 60.0f, 60.0f };
        room->wall_count = 4;
    }
}

int floor_find_room(const Floor *floor, int grid_x, int grid_y)
{
    for (int index = 0; index < floor->room_count; ++index) {
        if (floor->rooms[index].grid_x == grid_x && floor->rooms[index].grid_y == grid_y) {
            return index;
        }
    }
    return -1;
}

int floor_neighbor(const Floor *floor, int room_index, Direction direction)
{
    if (room_index < 0 || room_index >= floor->room_count ||
        direction < 0 || direction >= DIRECTION_COUNT) {
        return -1;
    }
    const Room *room = &floor->rooms[room_index];
    return floor_find_room(floor, room->grid_x + DIRECTION_X[direction],
                           room->grid_y + DIRECTION_Y[direction]);
}

static void connect_adjacent_rooms(Floor *floor)
{
    for (int index = 0; index < floor->room_count; ++index) {
        floor->rooms[index].door_mask = 0U;
        for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
            if (floor_neighbor(floor, index, (Direction)direction) >= 0) {
                floor->rooms[index].door_mask |= (uint8_t)(1U << (unsigned int)direction);
            }
        }
    }
}

static int choose_unassigned_room(const Floor *floor, Random *random, int excluded)
{
    int candidates[MAX_FLOOR_ROOMS];
    int count = 0;
    for (int index = 1; index < floor->room_count; ++index) {
        if (index != excluded && floor->rooms[index].type == ROOM_COMBAT) {
            candidates[count++] = index;
        }
    }
    return count == 0 ? -1 : candidates[random_range(random, count)];
}

static int choose_secret_room(const Floor *floor, Random *random, int excluded)
{
    int candidates[MAX_FLOOR_ROOMS];
    int count = 0;
    for (int index = 1; index < floor->room_count; ++index) {
        if (index == excluded || floor->rooms[index].type != ROOM_COMBAT) {
            continue;
        }
        int degree = 0;
        for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
            if (floor_neighbor(floor, index, (Direction)direction) >= 0) {
                ++degree;
            }
        }
        if (degree == 1) {
            candidates[count++] = index;
        }
    }
    return count == 0 ? -1 : candidates[random_range(random, count)];
}

static void assign_room_types(Floor *floor, Random *random)
{
    floor->rooms[0].type = ROOM_START;
    int boss_index = 1;
    int boss_distance = -1;
    for (int index = 1; index < floor->room_count; ++index) {
        Room *room = &floor->rooms[index];
        room->type = ROOM_COMBAT;
        int distance = abs(room->grid_x) + abs(room->grid_y);
        if (distance > boss_distance) {
            boss_distance = distance;
            boss_index = index;
        }
    }
    floor->rooms[boss_index].type = ROOM_BOSS;

    int reward = choose_unassigned_room(floor, random, boss_index);
    if (reward >= 0) {
        floor->rooms[reward].type = ROOM_REWARD;
    }
    int shop = choose_unassigned_room(floor, random, boss_index);
    if (shop >= 0) {
        floor->rooms[shop].type = ROOM_SHOP;
    }
    int secret = choose_secret_room(floor, random, boss_index);
    if (secret < 0) {
        secret = choose_unassigned_room(floor, random, boss_index);
    }
    if (secret >= 0) {
        floor->rooms[secret].type = ROOM_SECRET;
    }
}

static bool generate_layout(Floor *floor, Random *random)
{
    floor->room_count = 1;
    floor->rooms[0] = (Room) { .grid_x = 0, .grid_y = 0 };

    for (int room_index = 1; room_index < MAX_FLOOR_ROOMS; ++room_index) {
        bool placed = false;
        for (int attempt = 0; attempt < 128 && !placed; ++attempt) {
            int parent = random_range(random, floor->room_count);
            int direction = random_range(random, DIRECTION_COUNT);
            int x = floor->rooms[parent].grid_x + DIRECTION_X[direction];
            int y = floor->rooms[parent].grid_y + DIRECTION_Y[direction];
            int half_grid = FLOOR_GRID_SIZE / 2;
            if (x < -half_grid || x > half_grid || y < -half_grid || y > half_grid ||
                floor_find_room(floor, x, y) >= 0) {
                continue;
            }
            floor->rooms[floor->room_count++] = (Room) { .grid_x = x, .grid_y = y };
            placed = true;
        }
        if (!placed) {
            return false;
        }
    }
    return true;
}

static void generate_fallback(Floor *floor)
{
    static const int positions[MAX_FLOOR_ROOMS][2] = {
        { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 0, -1 },
        { 0, -2 }, { 0, -3 }, { -1, 0 }, { -2, 0 }, { -3, 0 },
    };
    floor->room_count = MAX_FLOOR_ROOMS;
    for (int index = 0; index < MAX_FLOOR_ROOMS; ++index) {
        floor->rooms[index] = (Room) {
            .grid_x = positions[index][0], .grid_y = positions[index][1]
        };
    }
}

bool floor_generate(Floor *floor, uint32_t seed)
{
    *floor = (Floor) { .seed = seed, .current_room = 0 };
    Random random;
    random_seed(&random, seed);
    if (!generate_layout(floor, &random)) {
        generate_fallback(floor);
    }
    connect_adjacent_rooms(floor);
    assign_room_types(floor, &random);

    for (int index = 0; index < floor->room_count; ++index) {
        Room *room = &floor->rooms[index];
        room->template_index = (uint8_t)random_range(&random, 3);
        if (room->type == ROOM_START) {
            room->template_index = 1U;
        }
        apply_template(room);
        room->cleared = room->type == ROOM_START || room->type == ROOM_REWARD ||
                        room->type == ROOM_SHOP || room->type == ROOM_SECRET;
        room->doors_open = room->cleared;
        room->visited = room->type == ROOM_START;
        room->revealed = room->type != ROOM_SECRET;
        if (room->type == ROOM_SECRET) {
            int degree = 0;
            for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
                if (floor_neighbor(floor, index, (Direction)direction) >= 0) {
                    ++degree;
                }
            }
            room->revealed = degree != 1;
        }
    }
    return floor_validate(floor);
}

bool floor_validate(const Floor *floor)
{
    if (floor->room_count != MAX_FLOOR_ROOMS || floor->current_room < 0 ||
        floor->current_room >= floor->room_count || floor->rooms[0].type != ROOM_START) {
        return false;
    }

    int type_counts[ROOM_BOSS + 1] = { 0 };
    bool reached[MAX_FLOOR_ROOMS] = { false };
    int queue[MAX_FLOOR_ROOMS];
    int head = 0;
    int tail = 0;
    reached[0] = true;
    queue[tail++] = 0;

    while (head < tail) {
        int index = queue[head++];
        const Room *room = &floor->rooms[index];
        if (room->type < ROOM_START || room->type > ROOM_BOSS ||
            room->wall_count < 0 || room->wall_count > MAX_WALLS) {
            return false;
        }
        ++type_counts[room->type];
        for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
            int neighbor = floor_neighbor(floor, index, (Direction)direction);
            bool has_door = (room->door_mask & (1U << (unsigned int)direction)) != 0U;
            if (has_door != (neighbor >= 0)) {
                return false;
            }
            if (neighbor >= 0 && !reached[neighbor]) {
                reached[neighbor] = true;
                queue[tail++] = neighbor;
            }
        }
    }

    for (int index = 0; index < floor->room_count; ++index) {
        if (!reached[index]) {
            return false;
        }
    }
    return type_counts[ROOM_START] == 1 && type_counts[ROOM_REWARD] == 1 &&
           type_counts[ROOM_SHOP] == 1 && type_counts[ROOM_SECRET] == 1 &&
           type_counts[ROOM_BOSS] == 1;
}

Room *floor_current_room(Floor *floor)
{
    return &floor->rooms[floor->current_room];
}

const Room *floor_current_room_const(const Floor *floor)
{
    return &floor->rooms[floor->current_room];
}

bool floor_connection_revealed(const Floor *floor, int room_index, Direction direction)
{
    int neighbor = floor_neighbor(floor, room_index, direction);
    return neighbor >= 0 && floor->rooms[neighbor].revealed;
}
