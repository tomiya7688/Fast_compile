#include "source.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_fscm_extension(const char *path)
{
    const size_t length = strlen(path);
    return length >= 5 && strcmp(path + length - 5, ".fscm") == 0;
}

static int reserve(unsigned char **data, size_t *capacity, size_t needed)
{
    size_t new_capacity = *capacity != 0 ? *capacity : 8192;
    unsigned char *grown;

    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2;
    }

    grown = realloc(*data, new_capacity);
    if (grown == NULL) {
        return 0;
    }
    *data = grown;
    *capacity = new_capacity;
    return 1;
}

FcSourceStatus fc_source_load(const char *path, FcSource *out)
{
    unsigned char chunk[8192];
    unsigned char *data = NULL;
    size_t length = 0;
    size_t capacity = 0;
    FILE *file;
    FcSourceStatus status = FC_SOURCE_OK;

    if (out == NULL) {
        return FC_SOURCE_INVALID_ARGUMENT;
    }
    out->data = NULL;
    out->length = 0;

    if (path == NULL || path[0] == '\0') {
        return FC_SOURCE_INVALID_ARGUMENT;
    }
    if (!has_fscm_extension(path)) {
        return FC_SOURCE_INVALID_EXTENSION;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return FC_SOURCE_OPEN_ERROR;
    }

    for (;;) {
        const size_t count = fread(chunk, 1, sizeof(chunk), file);
        if (count != 0) {
            /* Leave space for the convenience NUL terminator. */
            if (count >= SIZE_MAX - length) {
                status = FC_SOURCE_TOO_LARGE;
                break;
            }
            if (length + count + 1 > capacity &&
                !reserve(&data, &capacity, length + count + 1)) {
                status = FC_SOURCE_OUT_OF_MEMORY;
                break;
            }
            memcpy(data + length, chunk, count);
            length += count;
        }
        if (count < sizeof(chunk)) {
            if (ferror(file)) {
                status = FC_SOURCE_READ_ERROR;
            }
            break;
        }
    }
    if (status == FC_SOURCE_OK && data == NULL) {
        data = malloc(1);
        if (data == NULL) {
            status = FC_SOURCE_OUT_OF_MEMORY;
        }
    }
    if (status == FC_SOURCE_OK) {
        data[length] = '\0';
    }
    if (fclose(file) != 0 && status == FC_SOURCE_OK) {
        status = FC_SOURCE_READ_ERROR;
    }
    if (status != FC_SOURCE_OK) {
        free(data);
        return status;
    }

    out->data = data;
    out->length = length;
    return FC_SOURCE_OK;
}

void fc_source_free(FcSource *source)
{
    if (source != NULL) {
        free(source->data);
        source->data = NULL;
        source->length = 0;
    }
}

const char *fc_source_status_string(FcSourceStatus status)
{
    switch (status) {
    case FC_SOURCE_OK: return "ok";
    case FC_SOURCE_INVALID_ARGUMENT: return "invalid source path";
    case FC_SOURCE_INVALID_EXTENSION: return "expected a .fscm file";
    case FC_SOURCE_OPEN_ERROR: return "cannot open input file";
    case FC_SOURCE_READ_ERROR: return "cannot read input file";
    case FC_SOURCE_OUT_OF_MEMORY: return "out of memory while reading input";
    case FC_SOURCE_TOO_LARGE: return "input file is too large";
    }
    return "unknown source error";
}
