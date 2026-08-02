#include "save.h"

#include <ctype.h>
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

    char contents[256];
    size_t size = fread(contents, 1, sizeof(contents) - 1U, file);
    bool read_succeeded = !ferror(file) && feof(file);
    bool close_succeeded = fclose(file) == 0;
    contents[size] = '\0';
    if (!read_succeeded || !close_succeeded) {
        return false;
    }

    unsigned int version = 0U;
    unsigned int completed_runs = 0U;
    unsigned int boss_defeated = 0U;
    unsigned int reduced_flashes = 0U;
    int consumed = 0;
    if (sscanf(contents, "BOP_SAVE %u%n", &version, &consumed) != 1) {
        return false;
    }

    int fields = 0;
    if (version == 1U) {
        fields = sscanf(contents,
                        "BOP_SAVE %u\ncompleted_runs %u\nboss_defeated %u%n",
                        &version, &completed_runs, &boss_defeated, &consumed);
    } else if (version == SAVE_SCHEMA_VERSION) {
        fields = sscanf(contents,
                        "BOP_SAVE %u\ncompleted_runs %u\nboss_defeated %u\nreduced_flashes %u%n",
                        &version, &completed_runs, &boss_defeated,
                        &reduced_flashes, &consumed);
    }

    bool has_trailing_data = false;
    for (const char *cursor = contents + consumed; *cursor != '\0'; ++cursor) {
        if (!isspace((unsigned char)*cursor)) {
            has_trailing_data = true;
            break;
        }
    }
    int expected_fields = version == 1U ? 3 : 4;
    if (fields != expected_fields || has_trailing_data ||
        (version != 1U && version != SAVE_SCHEMA_VERSION) ||
        boss_defeated > 1U || reduced_flashes > 1U) {
        save_data_default(data);
        return false;
    }

    data->schema_version = SAVE_SCHEMA_VERSION;
    data->completed_runs = completed_runs;
    data->boss_defeated = boss_defeated != 0U;
    data->reduced_flashes = reduced_flashes != 0U;
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
    bool succeeded = fprintf(file,
                             "BOP_SAVE %u\ncompleted_runs %u\nboss_defeated %u\nreduced_flashes %u\n",
                             data->schema_version, data->completed_runs,
                             data->boss_defeated ? 1U : 0U,
                             data->reduced_flashes ? 1U : 0U) > 0;
    if (fclose(file) != 0) {
        succeeded = false;
    }
    if (!succeeded || rename(temporary_path, path) != 0) {
        (void)remove(temporary_path);
        return false;
    }
    return true;
}
