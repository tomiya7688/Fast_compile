#define _POSIX_C_SOURCE 200809L

#include "source_file.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
{
  "責務": {"fc_read_source": "入力をバッファに読み込みファイルの識別情報を返す"},
  "処理": ["入力を開き識別情報を取得する", "容量を増やしながら読み込む", "失敗時は資源を解放する"],
  "引数": {"path": "ソースパス", "source": "所有バッファの出力先", "length": "バイト数の出力先", "identity": "入力ファイルの識別情報の出力先"},
  "戻り値": ["全ファイルを読み込めたら真"]
}
*/
int fc_read_source(const char *path, char **source, size_t *length,
                   struct stat *identity) {
    FILE *input = fopen(path, "rb");
    if (input == NULL) {
        return 0;
    }
    if (fstat(fileno(input), identity) != 0) {
        fclose(input);
        return 0;
    }

    size_t capacity = 4096;
    size_t used = 0;
    char *buffer = malloc(capacity + 1);
    if (buffer == NULL) {
        fclose(input);
        return 0;
    }

    for (;;) {
        if (used == capacity) {
            if (capacity > (SIZE_MAX - 1) / 2) {
                free(buffer);
                fclose(input);
                return 0;
            }
            capacity *= 2;
            char *expanded = realloc(buffer, capacity + 1);
            if (expanded == NULL) {
                free(buffer);
                fclose(input);
                return 0;
            }
            buffer = expanded;
        }

        size_t count = fread(buffer + used, 1, capacity - used, input);
        used += count;
        if (count == 0) {
            if (ferror(input)) {
                free(buffer);
                fclose(input);
                return 0;
            }
            break;
        }
    }

    if (fclose(input) != 0) {
        free(buffer);
        return 0;
    }
    buffer[used] = '\0';
    *source = buffer;
    *length = used;
    return 1;
}
