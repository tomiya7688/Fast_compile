#include "compiler.h"

#include "elf_writer.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "source_file.h"

#include <stdlib.h>
#include <string.h>

/*
{
  責務: [
    fc_compile_file: CLIと後続compiler stageの間を接続する
  ]
  処理: [
    1: 引数と拡張子を検証する
    2: ソースを読み込み構文解析する
    3: ELF出力を生成し資源を解放する
  ]
  引数: [
    source_path: .fscm入力パス
    output_path: .o出力パス
    diagnostic: 呼び出し側へ返す診断内容
  ]
  戻り値: [
    status: compilerの終了状態
  ]
}
*/
FcStatus fc_compile_file(const char *source_path, const char *output_path,
                         FcDiagnostic *diagnostic) {
    diagnostic->line = 0;
    diagnostic->column = 0;
    diagnostic->path = NULL;
    diagnostic->message = "invalid compiler arguments";
    if (source_path == NULL || output_path == NULL ||
        strcmp(source_path, output_path) == 0) {
        return FC_INVALID_ARGUMENT;
    }

    size_t source_path_length = strlen(source_path);
    size_t output_path_length = strlen(output_path);
    if (source_path_length < 5 ||
        strcmp(source_path + source_path_length - 5, ".fscm") != 0 ||
        output_path_length < 3 ||
        strcmp(output_path + output_path_length - 2, ".o") != 0) {
        diagnostic->message = "expected .fscm input and .o output paths";
        return FC_INVALID_ARGUMENT;
    }

    char *source = NULL;
    size_t source_length = 0;
    if (!fc_read_source(source_path, &source, &source_length)) {
        diagnostic->path = source_path;
        diagnostic->message = "cannot read source file";
        return FC_INPUT_ERROR;
    }
    if (memchr(source, '\0', source_length) != NULL) {
        free(source);
        diagnostic->message = "source contains a NUL byte";
        return FC_SYNTAX_ERROR;
    }

    FcTokenList tokens = {0};
    if (!fc_lex_source(source, &tokens, diagnostic)) {
        free(source);
        return FC_SYNTAX_ERROR;
    }

    FcIRModule module = {0};
    diagnostic->path = source_path;
    if (!fc_parse_module(tokens.items, tokens.count, &module, diagnostic)) {
        fc_free_tokens(&tokens);
        fc_ir_free(&module);
        free(source);
        return FC_SYNTAX_ERROR;
    }
    fc_free_tokens(&tokens);

    if (!fc_write_elf_object(output_path, &module)) {
        fc_ir_free(&module);
        free(source);
        diagnostic->line = 0;
        diagnostic->column = 0;
        diagnostic->path = output_path;
        diagnostic->message = "cannot write object file";
        return FC_OUTPUT_ERROR;
    }
    fc_ir_free(&module);
    free(source);
    diagnostic->path = NULL;
    diagnostic->message = "";
    return FC_SUCCESS;
}
