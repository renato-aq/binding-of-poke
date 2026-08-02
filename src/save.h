#ifndef BIND_OF_POKE_SAVE_H
#define BIND_OF_POKE_SAVE_H

#include <stdbool.h>
#include <stdint.h>

enum { SAVE_SCHEMA_VERSION = 1 };

typedef struct SaveData {
    uint32_t schema_version;
    uint32_t completed_runs;
    bool boss_defeated;
} SaveData;

void save_data_default(SaveData *data);
bool save_data_load(const char *path, SaveData *data);
bool save_data_write(const char *path, const SaveData *data);

#endif
