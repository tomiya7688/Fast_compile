#ifndef FASTCOMPILE_PARSER_H
#define FASTCOMPILE_PARSER_H

#include "compiler.h"
#include "lexer.h"

typedef enum {
    FC_MODULE_EMPTY,
    FC_MAIN_VOID,
    FC_MAIN_INT_ZERO
} FcModuleKind;

/*
{
  責務: [
    fc_parse_module: サポートする最小のFast compileモジュール構文を解析する
  ]
  処理: [
    1: 空白を読み飛ばし関数宣言を確認する
    2: main関数の戻り値と本体を検証する
    3: 構文位置と結果を返す
  ]
  引数: [
    source: 解析する終端NUL付きソース
    kind: 解析したモジュール種別の出力先
    diagnostic: 構文エラー位置の出力先
  ]
  戻り値: [
    success: 構文が対応範囲内なら真
  ]
}
*/
int fc_parse_module(const FcToken *tokens, size_t count, FcModuleKind *kind,
                    FcDiagnostic *diagnostic);

#endif
