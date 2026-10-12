#include "parser.h"

#include <string.h>

typedef struct {
    const FcToken *tokens;
    size_t count;
    size_t index;
    FcDiagnostic *diagnostic;
} Parser;

/*
{
  責務: [
    current_token: token列の現在位置を返し末尾でも安全なtokenを選ぶ
  ]
  処理: [
    1: indexが有効なら該当tokenを返す
    2: 末尾以降なら終端tokenを返す
  ]
  引数: [
    parser: token列と現在index
  ]
  戻り値: [
    token: 現在位置のtoken
  ]
}
*/
static const FcToken *current_token(const Parser *parser) {
    return &parser->tokens[parser->index < parser->count
                               ? parser->index
                               : parser->count - 1];
}

/*
{
  責務: [
    fail_at_current: 現在tokenの位置と失敗理由をdiagnosticへ設定する
  ]
  処理: [
    1: current tokenのlineとcolumnをコピーする
    2: parser error messageを保存する
  ]
  引数: [
    parser: token列とdiagnostic
    message: 表示するエラー理由
  ]
  戻り値: [
    failure: parser失敗を示す0
  ]
}
*/
static int fail_at_current(Parser *parser, const char *message) {
    const FcToken *token = current_token(parser);
    parser->diagnostic->line = token->line;
    parser->diagnostic->column = token->column;
    parser->diagnostic->message = message;
    return 0;
}

/*
{
  責務: [
    consume_kind: 指定token種別を消費し不一致を診断する
  ]
  処理: [
    1: 現在tokenのkindを比較する
    2: 一致時だけindexを進める
    3: 不一致時に期待tokenを診断する
  ]
  引数: [
    parser: token列とdiagnostic
    kind: 消費するtoken種別
    message: 不一致時に出力する説明
  ]
  戻り値: [
    success: tokenを消費したら真
  ]
}
*/
static int consume_kind(Parser *parser, FcTokenKind kind,
                        const char *message) {
    if (current_token(parser)->kind != kind) {
        return fail_at_current(parser, message);
    }
    parser->index++;
    return 1;
}

/*
{
  責務: [
    consume_word: 現在のidentifierが指定keywordと一致するとき消費する
  ]
  処理: [
    1: token種別とspan長を確認する
    2: source spanをkeywordと比較する
    3: 一致時にindexを進める
  ]
  引数: [
    parser: token列とdiagnostic
    word: 期待するkeyword
  ]
  戻り値: [
    success: keywordを消費したら真
  ]
}
*/
static int consume_word(Parser *parser, const char *word) {
    const FcToken *token = current_token(parser);
    size_t length = strlen(word);
    if (token->kind != FC_TOKEN_IDENTIFIER || token->length != length ||
        memcmp(token->text, word, length) != 0) {
        const char *message = "expected 'return'";
        if (strcmp(word, "fn") == 0) {
            message = "expected 'fn'";
        } else if (strcmp(word, "main") == 0) {
            message = "expected 'main'";
        } else if (strcmp(word, "void") == 0) {
            message = "expected 'void'";
        } else if (strcmp(word, "int") == 0) {
            message = "expected 'int'";
        }
        return fail_at_current(parser, message);
    }
    parser->index++;
    return 1;
}

/*
{
  責務: [
    fc_parse_module: lexer token列から現在対応するmodule構文を検証する
  ]
  処理: [
    1: 終端tokenだけなら空moduleとして扱う
    2: main宣言・戻り値・本体をtoken単位で照合する
    3: 余分なtokenを拒否してparser位置を診断する
  ]
  引数: [
    tokens: EOFを含むlexer token列
    count: token配列要素数
    kind: 成功時のmodule種別
    diagnostic: 失敗位置と理由
  ]
  戻り値: [
    success: 全tokenを解析できたら真
  ]
}
*/
int fc_parse_module(const FcToken *tokens, size_t count, FcModuleKind *kind,
                    FcDiagnostic *diagnostic) {
    if (tokens == NULL || count == 0 || kind == NULL || diagnostic == NULL) {
        return 0;
    }
    if (tokens[count - 1].kind != FC_TOKEN_END) {
        diagnostic->line = tokens[count - 1].line;
        diagnostic->column = tokens[count - 1].column;
        diagnostic->message = "token stream has no end marker";
        return 0;
    }
    Parser parser = {tokens, count, 0, diagnostic};
    diagnostic->line = 1;
    diagnostic->column = 1;
    diagnostic->message = "unexpected token";
    if (current_token(&parser)->kind == FC_TOKEN_END) {
        if (count != 1) {
            return fail_at_current(&parser, "unexpected token after end marker");
        }
        *kind = FC_MODULE_EMPTY;
        return 1;
    }

    if (!consume_word(&parser, "fn") || !consume_word(&parser, "main") ||
        !consume_kind(&parser, FC_TOKEN_LEFT_PAREN, "expected '('") ||
        !consume_word(&parser, "void") ||
        !consume_kind(&parser, FC_TOKEN_RIGHT_PAREN, "expected ')'")) {
        return 0;
    }

    if (current_token(&parser)->kind == FC_TOKEN_ARROW) {
        parser.index++;
        if (!consume_word(&parser, "int") ||
            !consume_kind(&parser, FC_TOKEN_LEFT_BRACE, "expected '{'") ||
            !consume_word(&parser, "return")) {
            return 0;
        }
        const FcToken *return_value = current_token(&parser);
        if (return_value->kind != FC_TOKEN_INTEGER || return_value->length != 1 ||
            return_value->text[0] != '0') {
            return fail_at_current(&parser, "only return 0 is supported");
        }
        parser.index++;
        if (!consume_kind(&parser, FC_TOKEN_SEMICOLON, "expected ';'") ||
            !consume_kind(&parser, FC_TOKEN_RIGHT_BRACE, "expected '}'")) {
            return 0;
        }
        *kind = FC_MAIN_INT_ZERO;
    } else {
        if (!consume_kind(&parser, FC_TOKEN_LEFT_BRACE, "expected '{'") ||
            !consume_kind(&parser, FC_TOKEN_RIGHT_BRACE, "expected '}'")) {
            return 0;
        }
        *kind = FC_MAIN_VOID;
    }

    if (parser.index != count - 1 ||
        current_token(&parser)->kind != FC_TOKEN_END) {
        return fail_at_current(&parser,
                               "multiple declarations are not supported yet");
    }
    return 1;
}
