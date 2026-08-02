#include "save.h"

#include <stdio.h>
#include <string.h>

void save_data_default(SaveData *data)
{
    *data = (SaveData) { .schema_version = SAVE_SCHEMA_VERSION };
}

bool save_data_load(const char *path, SaveData *data)
{
    save_data_default(data);
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return false;
    }

    unsigned int version = 0U;
    unsigned int completed_runs = 0U;
    unsigned int boss_defeated = 0U;
    int fields = fscanf(file, "BOP_SAVE %u\ncompleted_runs %u\nboss_defeated %u",
                        &version, &completed_runs, &boss_defeated);
    char trailing = '\0';
    bool has_trailing_data = fscanf(file, " %c", &trailing) == 1;
    bool close_succeeded = fclose(file) == 0;
    if (fields != 3 || has_trailing_data || !close_succeeded ||
        version != SAVE_SCHEMA_VERSION ||
        boss_defeated > 1U) {
        save_data_default(data);
        return false;
    }

    data->schema_version = version;
    data->completed_runs = completed_runs;
    data->boss_defeated = boss_defeated != 0U;
    return true;
}

bool save_data_write(const char *path, const SaveData *data)
{
    if (data->schema_version != SAVE_SCHEMA_VERSION) {
        return false;
    }

    char temporary_path[512];
    int length = snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary_path)) {
        return false;
    }

    FILE *file = fopen(temporary_path, "w");
    if (file == NULL) {
        return false;
    }
    bool succeeded = fprintf(file, "BOP_SAVE %u\ncompleted_runs %u\nboss_defeated %u\n",
                             data->schema_version, data->completed_runs,
                             data->boss_defeated ? 1U : 0U) > 0;
    if (fclose(file) != 0) {
        succeeded = false;
    }
    if (!succeeded || rename(temporary_path, path) != 0) {
        (void)remove(temporary_path);
        return false;
    }
    return true;
}
