#ifndef FASTCOMPILE_COMPILER_H
#define FASTCOMPILE_COMPILER_H

typedef enum {
    FC_SUCCESS = 0,
    FC_INVALID_ARGUMENT = 2,
    FC_INPUT_ERROR = 3,
    FC_SYNTAX_ERROR = 4,
    FC_OUTPUT_ERROR = 5
} FcStatus;

typedef struct {
    unsigned line;
    unsigned column;
    const char *path;
    const char *message;
} FcDiagnostic;

/*
{
  責務: [
    fc_compile_file: .fscmソースを検証しLinux x86-64 ELFオブジェクトへ変換する
  ]
  処理: [
    1: 入力ファイルを読み込む
    2: ソースを構文解析する
    3: ELFオブジェクトを書き出す
  ]
  引数: [
    source_path: 入力ソースのパス
    output_path: 生成するELFオブジェクトのパス
    diagnostic: 失敗理由を受け取る診断情報
  ]
  戻り値: [
    status: 成否と終了コード
  ]
}
*/
FcStatus fc_compile_file(const char *source_path, const char *output_path,
                         FcDiagnostic *diagnostic);

#endif
