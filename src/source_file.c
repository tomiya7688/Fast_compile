#include "source_file.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
{
  責務: [
    fc_read_source: 入力ファイルをサイズ制限なしで安全にバッファへ蓄積する
  ]
  処理: [
    1: fopenの失敗を検出する
    2: 容量を倍増しながらfreadで読み込む
    3: I/Oまたは確保失敗時に資源を解放する
  ]
  引数: [
    path: 開くファイル名
    source: 成功時の所有バッファ出力先
    length: 読み込んだバイト数
  ]
  戻り値: [
    success: 全ファイルを読み込めたら真
  ]
}
*/
int fc_read_source(const char *path, char **source, size_t *length) {
    FILE *input = fopen(path, "rb");
    if (input == NULL) {
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
