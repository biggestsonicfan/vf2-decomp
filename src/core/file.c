#include "vf2/file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

vf2_status vf2_read_file(
    const char *path,
    uint8_t **data_out,
    size_t *size_out
)
{
    FILE *file = NULL;
    long length = 0;
    uint8_t *data = NULL;

    if (path == NULL || data_out == NULL || size_out == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return VF2_ERROR_IO;
    }

    length = ftell(file);
    if (length < 0) {
        fclose(file);
        return VF2_ERROR_IO;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return VF2_ERROR_IO;
    }

    data = (uint8_t *)malloc((size_t)length == 0u ? 1u : (size_t)length);
    if (data == NULL) {
        fclose(file);
        return VF2_ERROR_OUT_OF_MEMORY;
    }

    if ((size_t)length > 0u &&
        fread(data, 1u, (size_t)length, file) != (size_t)length) {
        free(data);
        fclose(file);
        return VF2_ERROR_IO;
    }

    if (fclose(file) != 0) {
        free(data);
        return VF2_ERROR_IO;
    }

    *data_out = data;
    *size_out = (size_t)length;
    return VF2_OK;
}

vf2_status vf2_write_file(
    const char *path,
    const void *data,
    size_t size
)
{
    FILE *file = NULL;

    if (path == NULL || (data == NULL && size != 0u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }

    if (size > 0u && fwrite(data, 1u, size, file) != size) {
        fclose(file);
        return VF2_ERROR_IO;
    }

    if (fclose(file) != 0) {
        return VF2_ERROR_IO;
    }

    return VF2_OK;
}

vf2_status vf2_make_directory(const char *path)
{
    int result = 0;

    if (path == NULL || path[0] == '\0') {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    result = _mkdir(path);
#else
    result = mkdir(path, 0755);
#endif

    if (result == 0 || errno == EEXIST) {
        return VF2_OK;
    }

    return VF2_ERROR_IO;
}

vf2_status vf2_make_directories(const char *path)
{
    char *copy = NULL;
    size_t length = 0u;
    size_t index = 0u;
    size_t start = 1u;
    vf2_status status = VF2_OK;

    if (path == NULL || path[0] == '\0') {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    length = strlen(path);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    memcpy(copy, path, length + 1u);

#ifdef _WIN32
    if (length >= 3u && copy[1] == ':' &&
        (copy[2] == '/' || copy[2] == '\\')) {
        start = 3u;
    }
#endif

    for (index = start; index < length; ++index) {
        if (copy[index] == '/' || copy[index] == '\\') {
            const char separator = copy[index];
            copy[index] = '\0';
            if (copy[0] != '\0') {
                status = vf2_make_directory(copy);
                if (status != VF2_OK) {
                    free(copy);
                    return status;
                }
            }
            copy[index] = separator;
        }
    }

    status = vf2_make_directory(copy);
    free(copy);
    return status;
}

vf2_status vf2_join_path(
    char *output,
    size_t output_size,
    const char *directory,
    const char *filename
)
{
    int written = 0;
    size_t length = 0u;
    const char *separator = "/";

    if (output == NULL || output_size == 0u ||
        directory == NULL || filename == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    length = strlen(directory);
    if (length > 0u &&
        (directory[length - 1u] == '/' || directory[length - 1u] == '\\')) {
        separator = "";
    }

    written = snprintf(
        output,
        output_size,
        "%s%s%s",
        directory,
        separator,
        filename
    );

    if (written < 0 || (size_t)written >= output_size) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }

    return VF2_OK;
}
