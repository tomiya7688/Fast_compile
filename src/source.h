#ifndef FASTC_SOURCE_H
#define FASTC_SOURCE_H

#include <stddef.h>

typedef struct {
    unsigned char *data;
    size_t length;
} FcSource;

typedef enum {
    FC_SOURCE_OK = 0,
    FC_SOURCE_INVALID_ARGUMENT,
    FC_SOURCE_INVALID_EXTENSION,
    FC_SOURCE_OPEN_ERROR,
    FC_SOURCE_READ_ERROR,
    FC_SOURCE_OUT_OF_MEMORY,
    FC_SOURCE_TOO_LARGE
} FcSourceStatus;

/* The returned buffer is length-delimited and also has a trailing NUL byte.
   A successful load does not imply that the contents are valid Fast compile. */
FcSourceStatus fc_source_load(const char *path, FcSource *out);
void fc_source_free(FcSource *source);
const char *fc_source_status_string(FcSourceStatus status);

#endif
