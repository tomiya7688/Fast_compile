#include "driver.h"
#include "source.h"

#include <stdio.h>
#include <string.h>

static void print_usage(FILE *stream)
{
    fputs("Usage: fastc --check-input <file.fscm>\n"
          "       fastc --help\n"
          "       fastc --version\n"
          "\n"
          "Bootstrap utility: --check-input reads bytes only; it does not parse or compile.\n",
          stream);
}

int fc_run_cli(int argc, char **argv)
{
    FcSource source = {0};
    FcSourceStatus status;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        fputs("fastc bootstrap (compiler not implemented)\n", stdout);
        return 0;
    }
    if (argc != 3 || strcmp(argv[1], "--check-input") != 0) {
        fputs("fastc: error: unsupported or incomplete command\n", stderr);
        print_usage(stderr);
        return 2;
    }

    status = fc_source_load(argv[2], &source);
    if (status != FC_SOURCE_OK) {
        fprintf(stderr, "fastc: error: %s: %s\n",
                argv[2], fc_source_status_string(status));
        return 3;
    }

    printf("fastc: read %zu bytes (syntax not checked)\n", source.length);
    fc_source_free(&source);
    return 0;
}
