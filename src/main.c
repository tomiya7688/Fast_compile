#include "compiler.h"

#include <stdio.h>

/*
{
  責務: [
    main: fastcompile CLI引数を検証してcompilerを実行する
  ]
  処理: [
    1: 入力と出力の引数形式を確認する
    2: コンパイラを呼び出す
    3: 失敗時に位置付き診断と終了コードを返す
  ]
  引数: [
    argc: CLI引数数
    argv: CLI引数配列
  ]
  戻り値: [
    exit_code: コンパイル結果
  ]
}
*/
int main(int argc, char **argv) {
    if (argc != 4 || argv[2][0] != '-' || argv[2][1] != 'o' ||
        argv[2][2] != '\0') {
        fputs("usage: fastcompile <source.fscm> -o <output.o>\n", stderr);
        return FC_INVALID_ARGUMENT;
    }

    FcDiagnostic diagnostic;
    FcStatus status = fc_compile_file(argv[1], argv[3], &diagnostic);
    if (status != FC_SUCCESS) {
        if (diagnostic.line != 0 && diagnostic.path != NULL) {
            fprintf(stderr, "fastcompile: %s:%u:%u: error: %s\n", argv[1],
                    diagnostic.line, diagnostic.column, diagnostic.message);
        } else if (diagnostic.path != NULL) {
            fprintf(stderr, "fastcompile: error: %s: %s\n", diagnostic.path,
                    diagnostic.message);
        } else {
            fprintf(stderr, "fastcompile: error: %s\n", diagnostic.message);
        }
    }
    return status;
}
