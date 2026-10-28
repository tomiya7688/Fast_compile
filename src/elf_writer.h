#ifndef FASTCOMPILE_ELF_WRITER_H
#define FASTCOMPILE_ELF_WRITER_H

#include "ir.h"

/*
{
  "責務": {"fc_write_elf_object": "関数IRをLinux x86-64 ELFオブジェクトとして保存する"},
  "処理": ["機械語とシンボルの配置を計算する", "ELFヘッダーを組み立てて出力する"],
  "引数": {"path": "生成先", "module": "関数IR列"},
  "戻り値": ["書き終えたら真"]
}
*/
int fc_write_elf_object(const char *path, const FcIRModule *module);

#endif
