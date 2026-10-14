#ifndef FASTCOMPILE_PARSER_H
#define FASTCOMPILE_PARSER_H

#include "compiler.h"
#include "ir.h"
#include "lexer.h"

/*
{
  責務: [
    fc_parse_module: lexer token列をbackend非依存の関数IRへ変換する
  ]
  処理: [
    1: 関数名・引数・戻り値をtoken単位で解析する
    2: 空関数またはreturn 0のterminatorをIRへ追加する
    3: 重複名と未対応構文を位置付きで拒否する
  ]
  引数: [
    tokens: EOFを含むlexer token列
    module: 0初期化済みmodule IRの出力先
    diagnostic: 構文エラー位置の出力先
  ]
  戻り値: [
    success: 構文が対応範囲内なら真
  ]
}
*/
int fc_parse_module(const FcToken *tokens, size_t count, FcIRModule *module,
                    FcDiagnostic *diagnostic);

#endif
