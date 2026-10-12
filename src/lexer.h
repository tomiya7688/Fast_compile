#ifndef FASTCOMPILE_LEXER_H
#define FASTCOMPILE_LEXER_H

#include "compiler.h"

#include <stddef.h>

typedef enum {
    FC_TOKEN_IDENTIFIER,
    FC_TOKEN_INTEGER,
    FC_TOKEN_LEFT_PAREN,
    FC_TOKEN_RIGHT_PAREN,
    FC_TOKEN_LEFT_BRACE,
    FC_TOKEN_RIGHT_BRACE,
    FC_TOKEN_SEMICOLON,
    FC_TOKEN_ARROW,
    FC_TOKEN_END
} FcTokenKind;

typedef struct {
    FcTokenKind kind;
    const char *text;
    size_t length;
    unsigned line;
    unsigned column;
} FcToken;

typedef struct {
    FcToken *items;
    size_t count;
    size_t capacity;
} FcTokenList;

/*
{
  責務: [
    fc_lex_source: ソースをparserが扱えるtoken列へ変換する
  ]
  処理: [
    1: 識別子・整数・対応記号を一度だけ走査する
    2: token位置を記録し動的配列へ格納する
    3: 不正文字または確保失敗の位置を診断する
  ]
  引数: [
    source: NUL終端済みソース
    tokens: token列の出力先
    diagnostic: lexer診断の出力先
  ]
  戻り値: [
    success: 全ソースをtoken化できたら真
  ]
}
*/
int fc_lex_source(const char *source, FcTokenList *tokens,
                  FcDiagnostic *diagnostic);

/*
{
  責務: [
    fc_free_tokens: token列が所有する配列を解放して空状態へ戻す
  ]
  処理: [
    1: token配列を解放する
    2: pointer・count・capacityを初期状態へ戻す
  ]
  引数: [
    tokens: 解放するtoken列
  ]
}
*/
void fc_free_tokens(FcTokenList *tokens);

#endif
