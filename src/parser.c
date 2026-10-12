#include "parser.h"

#include <ctype.h>
#include <string.h>

typedef struct {
    const char *source;
    size_t offset;
    unsigned line;
    unsigned column;
    FcDiagnostic *diagnostic;
} Parser;

/*
{
  責務: [
    advance: 次の非空白文字へ移動し現在位置を保つ
  ]
  処理: [
    1: 空白を1文字ずつ進める
    2: 改行では行番号を更新する
    3: 通常文字では列番号を更新する
  ]
  引数: [
    parser: 走査状態
  ]
  戻り値: [
    character: 次の非空白文字
  ]
}
*/
static char advance(Parser *parser) {
    while (isspace((unsigned char)parser->source[parser->offset])) {
        if (parser->source[parser->offset] == '\n') {
            parser->line++;
            parser->column = 1;
        } else {
            parser->column++;
        }
        parser->offset++;
    }
    return parser->source[parser->offset];
}

/*
{
  責務: [
    expect: 次の文字が期待値と一致することを検証する
  ]
  処理: [
    1: 空白を読み飛ばす
    2: 不一致なら位置付き診断を設定する
    3: 一致時は入力位置を進める
  ]
  引数: [
    parser: 解析状態
    expected: 必須文字
  ]
  戻り値: [
    success: 文字が一致したら真
  ]
}
*/
static int expect(Parser *parser, char expected) {
    char actual = advance(parser);
    if (actual != expected) {
        parser->diagnostic->line = parser->line;
        parser->diagnostic->column = parser->column;
        parser->diagnostic->message = "unexpected token";
        return 0;
    }
    parser->offset++;
    parser->column++;
    return 1;
}

/*
{
  責務: [
    expect_word: 指定された識別子と境界を検証する
  ]
  処理: [
    1: 空白を読み飛ばす
    2: 識別子の文字列と比較する
    3: 識別子の直後が英数字でないことを確認する
  ]
  引数: [
    parser: 解析状態
    word: 期待するキーワード
  ]
  戻り値: [
    success: キーワードが一致したら真
  ]
}
*/
static int expect_word(Parser *parser, const char *word) {
    advance(parser);
    size_t length = strlen(word);
    if (strncmp(parser->source + parser->offset, word, length) != 0 ||
        (isalnum((unsigned char)parser->source[parser->offset + length]) ||
         parser->source[parser->offset + length] == '_')) {
        parser->diagnostic->line = parser->line;
        parser->diagnostic->column = parser->column;
        parser->diagnostic->message = "unsupported declaration";
        return 0;
    }
    parser->offset += length;
    parser->column += (unsigned)length;
    return 1;
}

/*
{
  責務: [
    fc_parse_module: 空モジュールと仕様で定義されたmain形式を解析する
  ]
  処理: [
    1: 空モジュールなら空種別を返す
    2: main宣言の引数・戻り値・本体を照合する
    3: 未対応構文や余分な宣言を拒否する
  ]
  引数: [
    source: NUL終端済み入力
    kind: 成功時のモジュール種別
    diagnostic: 構文エラー詳細
  ]
  戻り値: [
    success: 全入力を解析できたら真
  ]
}
*/
int fc_parse_module(const char *source, FcModuleKind *kind,
                    FcDiagnostic *diagnostic) {
    Parser parser = {source, 0, 1, 1, diagnostic};
    diagnostic->line = 1;
    diagnostic->column = 1;
    diagnostic->message = "unexpected token";
    if (advance(&parser) == '\0') {
        *kind = FC_MODULE_EMPTY;
        return 1;
    }

    if (!expect_word(&parser, "fn") || !expect_word(&parser, "main") ||
        !expect(&parser, '(') || !expect_word(&parser, "void") ||
        !expect(&parser, ')')) {
        return 0;
    }

    char next = advance(&parser);
    if (next == '-') {
        parser.offset++;
        parser.column++;
        if (!expect(&parser, '>') || !expect_word(&parser, "int") ||
            !expect(&parser, '{') || !expect_word(&parser, "return") ||
            !expect(&parser, '0') || !expect(&parser, ';') ||
            !expect(&parser, '}')) {
            return 0;
        }
        *kind = FC_MAIN_INT_ZERO;
    } else {
        if (!expect(&parser, '{') || !expect(&parser, '}')) {
            return 0;
        }
        *kind = FC_MAIN_VOID;
    }

    if (advance(&parser) != '\0') {
        diagnostic->line = parser.line;
        diagnostic->column = parser.column;
        diagnostic->message = "multiple declarations are not supported yet";
        return 0;
    }
    return 1;
}
