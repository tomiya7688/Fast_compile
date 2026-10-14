#include "lexer.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    const char *source;
    size_t offset;
    unsigned line;
    unsigned column;
    FcTokenList *tokens;
    FcDiagnostic *diagnostic;
} Lexer;

/*
{
  責務: [
    is_identifier_start: ASCII識別子の先頭として許可する文字を判定する
  ]
  処理: [
    1: 英字またはunderscoreだけを受け付ける
  ]
  引数: [
    character: 判定対象のbyte
  ]
  戻り値: [
    allowed: 識別子の先頭にできる場合は真
  ]
}
*/
static int is_identifier_start(unsigned char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') || character == '_';
}

/*
{
  責務: [
    is_identifier_continue: ASCII識別子の継続文字を判定する
  ]
  処理: [
    1: 識別子先頭文字または数字を受け付ける
  ]
  引数: [
    character: 判定対象のbyte
  ]
  戻り値: [
    allowed: 識別子内で許可する場合は真
  ]
}
*/
static int is_identifier_continue(unsigned char character) {
    return is_identifier_start(character) ||
           (character >= '0' && character <= '9');
}

/*
{
  責務: [
    is_digit: ASCII十進数字か判定する
  ]
  処理: [
    1: '0'から'9'の範囲を比較する
  ]
  引数: [
    character: 判定対象のbyte
  ]
  戻り値: [
    digit: 十進数字なら真
  ]
}
*/
static int is_digit(unsigned char character) {
    return character >= '0' && character <= '9';
}

/*
{
  責務: [
    is_whitespace: source grammarで扱うASCII空白を判定する
  ]
  処理: [
    1: space・tab・改行系の6文字を比較する
  ]
  引数: [
    character: 判定対象のbyte
  ]
  戻り値: [
    whitespace: 空白なら真
  ]
}
*/
static int is_whitespace(char character) {
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\v' || character == '\f';
}

/*
{
  責務: [
    advance: lexer位置を1byte進めlineとcolumnを更新する
  ]
  処理: [
    1: offsetを進める
    2: LFならlineを増やしcolumnを戻す
    3: それ以外はcolumnを増やす
  ]
  引数: [
    lexer: 走査状態
  ]
  戻り値: [
    character: 進める前のbyte
  ]
}
*/
static char advance(Lexer *lexer) {
    char character = lexer->source[lexer->offset++];
    if (character == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return character;
}

/*
{
  責務: [
    append_token: tokenを配列へ追加し必要な場合だけ容量を増やす
  ]
  処理: [
    1: 容量不足を確認する
    2: overflowを避けて容量を倍増する
    3: tokenのkind・span・source位置を記録する
  ]
  引数: [
    lexer: token出力と診断を持つ走査状態
    kind: 追加するtoken種別
    start: token先頭offset
    length: token byte数
    line: token開始行
    column: token開始列
  ]
  戻り値: [
    success: tokenを追加できたら真
  ]
}
*/
static int append_token(Lexer *lexer, FcTokenKind kind, size_t start,
                        size_t length, unsigned line, unsigned column) {
    FcTokenList *tokens = lexer->tokens;
    if (tokens->count == tokens->capacity) {
        size_t capacity = tokens->capacity == 0 ? 32 : tokens->capacity * 2;
        if (capacity < tokens->capacity || capacity > SIZE_MAX / sizeof(FcToken)) {
            lexer->diagnostic->line = line;
            lexer->diagnostic->column = column;
            lexer->diagnostic->message = "token list is too large";
            return 0;
        }
        FcToken *items = realloc(tokens->items, capacity * sizeof(FcToken));
        if (items == NULL) {
            lexer->diagnostic->line = line;
            lexer->diagnostic->column = column;
            lexer->diagnostic->message = "cannot allocate token list";
            return 0;
        }
        tokens->items = items;
        tokens->capacity = capacity;
    }

    tokens->items[tokens->count++] =
        (FcToken){kind, lexer->source + start, length, line, column};
    return 1;
}

/*
{
  責務: [
    fc_lex_source: sourceを識別子・整数・対応punctuationとEOFへtoken化する
  ]
  処理: [
    1: 空白を飛ばしtoken開始位置を記録する
    2: 1byte走査でspanを作りtoken配列へ追加する
    3: 不正文字で位置付き診断を返す
  ]
  引数: [
    source: NUL終端済みsource text
    tokens: 0初期化済みtoken list
    diagnostic: 失敗理由の出力先
  ]
  戻り値: [
    success: EOFまで走査できたら真
  ]
}
*/
int fc_lex_source(const char *source, FcTokenList *tokens,
                  FcDiagnostic *diagnostic) {
    if (tokens == NULL || diagnostic == NULL) {
        return 0;
    }
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
    if (source == NULL) {
        diagnostic->line = 1;
        diagnostic->column = 1;
        diagnostic->message = "source is null";
        return 0;
    }

    Lexer lexer = {source, 0, 1, 1, tokens, diagnostic};

    while (source[lexer.offset] != '\0') {
        if (is_whitespace(source[lexer.offset])) {
            advance(&lexer);
            continue;
        }

        size_t start = lexer.offset;
        unsigned line = lexer.line;
        unsigned column = lexer.column;
        unsigned char character = (unsigned char)source[lexer.offset];
        FcTokenKind kind;

        if (is_identifier_start(character)) {
            kind = FC_TOKEN_IDENTIFIER;
            do {
                advance(&lexer);
            } while (is_identifier_continue((unsigned char)source[lexer.offset]));
        } else if (is_digit(character)) {
            kind = FC_TOKEN_INTEGER;
            do {
                advance(&lexer);
            } while (is_digit((unsigned char)source[lexer.offset]));
        } else {
            advance(&lexer);
            switch (character) {
            case '(':
                kind = FC_TOKEN_LEFT_PAREN;
                break;
            case ')':
                kind = FC_TOKEN_RIGHT_PAREN;
                break;
            case '{':
                kind = FC_TOKEN_LEFT_BRACE;
                break;
            case '}':
                kind = FC_TOKEN_RIGHT_BRACE;
                break;
            case ';':
                kind = FC_TOKEN_SEMICOLON;
                break;
            case '-':
                if (source[lexer.offset] != '>') {
                    diagnostic->line = line;
                    diagnostic->column = column;
                    diagnostic->message = "expected '>' after '-'";
                    fc_free_tokens(tokens);
                    return 0;
                }
                advance(&lexer);
                kind = FC_TOKEN_ARROW;
                break;
            default:
                diagnostic->line = line;
                diagnostic->column = column;
                diagnostic->message = "invalid source character";
                fc_free_tokens(tokens);
                return 0;
            }
        }

        if (!append_token(&lexer, kind, start, lexer.offset - start, line,
                          column)) {
            fc_free_tokens(tokens);
            return 0;
        }
    }

    if (!append_token(&lexer, FC_TOKEN_END, lexer.offset, 0, lexer.line,
                      lexer.column)) {
        fc_free_tokens(tokens);
        return 0;
    }
    return 1;
}

/*
{
  責務: [
    fc_free_tokens: lexer token配列を解放する
  ]
  処理: [
    1: token配列を解放する
    2: listを再利用可能な空状態にする
  ]
  引数: [
    tokens: 解放対象
  ]
}
*/
void fc_free_tokens(FcTokenList *tokens) {
    if (tokens == NULL) {
        return;
    }
    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}
