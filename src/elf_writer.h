#ifndef FASTCOMPILE_ELF_WRITER_H
#define FASTCOMPILE_ELF_WRITER_H

#include "ir.h"
#include <sys/stat.h>

typedef enum {
    FC_ELF_WRITE_ERROR = 0,
    FC_ELF_WRITE_OK = 1,
    FC_ELF_WRITE_SAME_FILE = 2
} FcElfWriteResult;

/*
{
  "責務": {"fc_write_elf_object": "関数IRをLinux x86-64 ELFオブジェクトとして保存する"},
  "処理": ["機械語とシンボルの配置を計算する", "ELFヘッダーを組み立てて出力する"],
  "引数": {"path": "生成先", "module": "関数IR列", "source_identity": "読み取った入力ファイルの識別情報"},
  "戻り値": ["成功、出力失敗、入力と同一のいずれか"]
}
*/
FcElfWriteResult fc_write_elf_object(const char *path, const FcIRModule *module,
                                     const struct stat *source_identity);

#endif
