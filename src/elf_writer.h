#ifndef FASTCOMPILE_ELF_WRITER_H
#define FASTCOMPILE_ELF_WRITER_H

#include "parser.h"

/*
{
  責務: [
    fc_write_elf_object: モジュール種別をLinux x86-64 ELF relocatable objectとして保存する
  ]
  処理: [
    1: .textとsymbol/string tableの配置を計算する
    2: ELF headerとsection headerを組み立てる
    3: 指定した出力先への書き込み完了を確認する
  ]
  引数: [
    path: 生成先のobject file
    kind: コード生成対象のモジュール種別
  ]
  戻り値: [
    success: object fileを書き終えたら真
  ]
}
*/
int fc_write_elf_object(const char *path, FcModuleKind kind);

#endif
