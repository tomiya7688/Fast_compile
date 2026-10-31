#include "parser.h"

#include <string.h>

typedef struct {
    const FcToken *tokens;
    size_t count;
    size_t index;
    FcIRModule *module;
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
    return &parser->tokens[parser->index < parser->count ? parser->index
                                                         : parser->count - 1];
}

/*
{
  責務: [
    fail_at: 指定token位置と失敗理由をdiagnosticへ設定する
  ]
  処理: [
    1: lineとcolumnをtokenから取得する
    2: parser error messageを保存する
  ]
  引数: [
    parser: token列とdiagnostic
    token: エラー位置
    message: 表示する理由
  ]
  戻り値: [
    failure: parser失敗を示す0
  ]
}
*/
static int fail_at(Parser *parser, const FcToken *token, const char *message) {
    parser->diagnostic->line = token->line;
    parser->diagnostic->column = token->column;
    parser->diagnostic->message = message;
    return 0;
}

/*
{
  責務: [
    fail_current: 現在token位置にparser errorを設定する
  ]
  処理: [
    1: current tokenを取得する
    2: fail_atへ理由を渡す
  ]
  引数: [
    parser: token列とdiagnostic
    message: 表示する理由
  ]
  戻り値: [
    failure: parser失敗を示す0
  ]
}
*/
static int fail_current(Parser *parser, const char *message) {
    return fail_at(parser, current_token(parser), message);
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
static int consume_kind(Parser *parser, FcTokenKind kind, const char *message) {
    if (current_token(parser)->kind != kind) {
        return fail_current(parser, message);
    }
    parser->index++;
    return 1;
}

/*
{
  責務: [
    is_word: identifier tokenが指定文字列と同じか判定する
  ]
  処理: [
    1: token kindとspan長を確認する
    2: token spanのbyte列を比較する
  ]
  引数: [
    token: 比較対象
    word: 期待するidentifier
  ]
  戻り値: [
    equal: 完全一致したら真
  ]
}
*/
static int is_word(const FcToken *token, const char *word) {
    size_t length = strlen(word);
    return token->kind == FC_TOKEN_IDENTIFIER && token->length == length &&
           memcmp(token->text, word, length) == 0;
}

/*
{
  責務: [
    consume_word: 現在のidentifierが指定keywordと一致するとき消費する
  ]
  処理: [
    1: keywordのspan一致を確認する
    2: 一致時にindexを進める
    3: 不一致時に期待keywordを診断する
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
    if (!is_word(current_token(parser), word)) {
        const char *message = "expected 'return'";
        if (strcmp(word, "fn") == 0) {
            message = "expected 'fn'";
        } else if (strcmp(word, "void") == 0) {
            message = "expected 'void'";
        } else if (strcmp(word, "int") == 0) {
            message = "expected 'int'";
        }
        return fail_current(parser, message);
    }
    parser->index++;
    return 1;
}

/*
{
  "責務": {"parse_int_literal": "符号付きintリテラルを範囲検査して読み取る"},
  "処理": ["整数トークンを確認する", "32ビット符号付き範囲を超えないよう各桁を加算する"],
  "引数": {"parser": "トークン列と診断", "value": "読み取った値の出力先"},
  "戻り値": ["範囲内の整数を消費できたら真"]
}
*/
static int parse_int_literal(Parser *parser, int64_t *value) {
    int negative = current_token(parser)->kind == FC_TOKEN_MINUS;
    if (negative) {
        parser->index++;
    }
    const FcToken *token = current_token(parser);
    if (token->kind != FC_TOKEN_INTEGER || token->length == 0) {
        return fail_current(parser, "expected integer literal");
    }
    int64_t number = 0;
    int64_t limit = (int64_t)INT32_MAX + negative;
    for (size_t index = 0; index < token->length; index++) {
        char digit = token->text[index];
        if (digit < '0' || digit > '9') {
            return fail_current(parser, "invalid integer literal");
        }
        int64_t part = digit - '0';
        if (number > (limit - part) / 10) {
            return fail_current(parser, "integer literal exceeds int range");
        }
        number = number * 10 + part;
    }
    *value = negative ? -number : number;
    parser->index++;
    return 1;
}

/*
{
  "責務": {"parse_function": "関数定義を検証して関数IRに追加する"},
  "処理": ["関数名と引数を読み取る", "戻り値と関数本体を検証する", "重複を確認してIRに追加する"],
  "引数": {"parser": "トークン列、IR、診断"},
  "戻り値": ["関数定義を追加できたら真"]
}
*/
static int parse_function(Parser *parser) {
    if (!consume_word(parser, "fn")) {
        return 0;
    }

    const FcToken *name = current_token(parser);
    if (name->kind != FC_TOKEN_IDENTIFIER) {
        return fail_current(parser, "expected function name");
    }
    parser->index++;

    if (!consume_kind(parser, FC_TOKEN_LEFT_PAREN, "expected '('")) {
        return 0;
    }
    if (is_word(current_token(parser), "void")) {
        parser->index++;
    }
    if (!consume_kind(parser, FC_TOKEN_RIGHT_PAREN, "expected ')'")) {
        return 0;
    }

    int returns_int = 0;
    int explicit_return_type = 0;
    if (current_token(parser)->kind == FC_TOKEN_ARROW) {
        parser->index++;
        explicit_return_type = 1;
        if (is_word(current_token(parser), "int")) {
            returns_int = 1;
            parser->index++;
        } else if (is_word(current_token(parser), "void")) {
            parser->index++;
        } else {
            return fail_current(parser, "expected return type 'void' or 'int'");
        }
    }

    if (is_word(name, "main") && explicit_return_type && !returns_int) {
        return fail_at(parser, name,
                       "main must use one of the specified entry signatures");
    }

    if (!consume_kind(parser, FC_TOKEN_LEFT_BRACE, "expected '{'")) {
        return 0;
    }

    FcIRTerminator terminator = FC_IR_RETURN_VOID;
    int64_t return_value = 0;
    if (returns_int) {
        if (!consume_word(parser, "return")) {
            return 0;
        }
        if (!parse_int_literal(parser, &return_value)) {
            return 0;
        }
        if (!consume_kind(parser, FC_TOKEN_SEMICOLON, "expected ';'")) {
            return 0;
        }
        terminator = FC_IR_RETURN_CONSTANT;
    }

    if (!consume_kind(parser, FC_TOKEN_RIGHT_BRACE, "expected '}'")) {
        return 0;
    }

    FcIRFunction function = {name->text, name->length, terminator,
                             return_value};
    FcIRAddResult result = fc_ir_add_function(parser->module, function);
    if (result == FC_IR_ADD_DUPLICATE_NAME) {
        return fail_at(parser, name, "duplicate function name");
    }
    if (result == FC_IR_ADD_INVALID_FUNCTION) {
        return fail_at(parser, name, "invalid function IR");
    }
    if (result == FC_IR_ADD_ALLOCATION_ERROR) {
        return fail_at(parser, name, "cannot allocate function IR");
    }
    return 1;
}

/*
{
  責務: [
    fc_parse_module: token列からfunction IR列を構築する
  ]
  処理: [
    1: EOF markerと空moduleを確認する
    2: EOFまでfunction definitionをsource順で解析する
    3: syntax error時は診断位置を保存する
  ]
  引数: [
    tokens: EOFを含むlexer token列
    count: token要素数
    module: 0初期化済みmodule IRの出力先
    diagnostic: 失敗位置と理由
  ]
  戻り値: [
    success: 全tokenを関数IRへ変換できたら真
  ]
}
*/
int fc_parse_module(const FcToken *tokens, size_t count, FcIRModule *module,
                    FcDiagnostic *diagnostic) {
    if (tokens == NULL || count == 0 || module == NULL || diagnostic == NULL) {
        return 0;
    }
    if (tokens[count - 1].kind != FC_TOKEN_END) {
        diagnostic->line = tokens[count - 1].line;
        diagnostic->column = tokens[count - 1].column;
        diagnostic->message = "token stream has no end marker";
        return 0;
    }

    Parser parser = {tokens, count, 0, module, diagnostic};
    diagnostic->line = 1;
    diagnostic->column = 1;
    diagnostic->message = "unexpected token";
    while (current_token(&parser)->kind != FC_TOKEN_END) {
        if (!parse_function(&parser)) {
            return 0;
        }
    }

    if (parser.index != count - 1) {
        return fail_current(&parser, "unexpected token after end marker");
    }
    return 1;
}
