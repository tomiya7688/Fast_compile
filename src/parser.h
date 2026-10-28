#ifndef FASTCOMPILE_PARSER_H
#define FASTCOMPILE_PARSER_H

#include "compiler.h"
#include "ir.h"
#include "lexer.h"

/*
{
  "責務": {"fc_parse_module": "トークン列をバックエンド非依存の関数IRへ変換する"},
  "処理": ["関数宣言を解析する", "戻り値をIRへ追加する", "重複名と未対応構文を診断する"],
  "引数": {"tokens": "終端を含むトークン列", "count": "トークン数", "module": "ゼロ初期化済みIR", "diagnostic": "診断の出力先"},
  "戻り値": ["構文が対応範囲内なら真"]
}
*/
int fc_parse_module(const FcToken *tokens, size_t count, FcIRModule *module,
                    FcDiagnostic *diagnostic);

#endif
