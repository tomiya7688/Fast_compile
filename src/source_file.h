#ifndef FASTCOMPILE_SOURCE_FILE_H
#define FASTCOMPILE_SOURCE_FILE_H

#include <stddef.h>

/*
{
  責務: [
    fc_read_source: ソース全体を終端NUL付きメモリへ読み込む
  ]
  処理: [
    1: ファイルをバイナリモードで開く
    2: 必要量に応じてバッファを拡張する
    3: 読み取り完了後に終端NULを付ける
  ]
  引数: [
    path: 読み込むソースパス
    source: ソースバッファの出力先
    length: NULを除くソースサイズの出力先
  ]
  戻り値: [
    success: 読み込みに成功したら真
  ]
}
*/
int fc_read_source(const char *path, char **source, size_t *length);

#endif
