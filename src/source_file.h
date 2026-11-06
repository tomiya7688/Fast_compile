#ifndef FASTCOMPILE_SOURCE_FILE_H
#define FASTCOMPILE_SOURCE_FILE_H

#include <stddef.h>
#include <sys/stat.h>

/*
{
  "責務": {"fc_read_source": "ソースを終端NUL付きメモリに読み込む"},
  "処理": ["入力を開いて識別情報を取得する", "内容を読み込み終端NULを付ける"],
  "引数": {"path": "ソースパス", "source": "バッファ出力先", "length": "サイズ出力先", "identity": "入力ファイルの識別情報の出力先"},
  "戻り値": ["読み込みに成功したら真"]
}
*/
int fc_read_source(const char *path, char **source, size_t *length,
                   struct stat *identity);

#endif
