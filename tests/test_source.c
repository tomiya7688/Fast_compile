#include "source.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "test_source:%d: %s\n", __LINE__, #cond); \
        return 1; \
    } \
} while (0)

int main(void)
{
    FcSource source = {0};
    const char expected[] = "fn main(void) {\n}\n";

    CHECK(fc_source_load("tests/fixtures/empty.fscm", &source) == FC_SOURCE_OK);
    CHECK(source.data != NULL);
    CHECK(source.length == 0);
    CHECK(source.data[0] == '\0');
    fc_source_free(&source);
    CHECK(source.data == NULL && source.length == 0);

    CHECK(fc_source_load("tests/fixtures/minimal.fscm", &source) == FC_SOURCE_OK);
    CHECK(source.length == sizeof(expected) - 1);
    CHECK(memcmp(source.data, expected, sizeof(expected)) == 0);
    fc_source_free(&source);

    CHECK(fc_source_load("tests/fixtures/missing.fscm", &source) == FC_SOURCE_OPEN_ERROR);
    CHECK(source.data == NULL && source.length == 0);
    CHECK(fc_source_load("README.md", &source) == FC_SOURCE_INVALID_EXTENSION);
    CHECK(fc_source_load(NULL, &source) == FC_SOURCE_INVALID_ARGUMENT);
    CHECK(fc_source_load("tests/fixtures/empty.fscm", NULL) == FC_SOURCE_INVALID_ARGUMENT);

    puts("test_source: ok");
    return 0;
}
