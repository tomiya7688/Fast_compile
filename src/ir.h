#ifndef FASTCOMPILE_IR_H
#define FASTCOMPILE_IR_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    FC_IR_RETURN_VOID,
    FC_IR_RETURN_CONSTANT
} FcIRTerminator;

typedef struct {
    const char *name;
    size_t name_length;
    FcIRTerminator terminator;
    int64_t return_value;
} FcIRFunction;

typedef struct {
    FcIRFunction *functions;
    size_t count;
    size_t capacity;
} FcIRModule;

typedef enum {
    FC_IR_ADD_SUCCESS,
    FC_IR_ADD_DUPLICATE_NAME,
    FC_IR_ADD_INVALID_FUNCTION,
    FC_IR_ADD_ALLOCATION_ERROR
} FcIRAddResult;

/*
{
  責務: [
    fc_ir_add_function: borrowed name spanを持つ関数IRをmoduleへ追加する
  ]
  処理: [
    1: 既存関数名との重複を確認する
    2: 必要時だけ関数配列を拡張する
    3: 関数IRをsource順で配列末尾へ格納する
  ]
  引数: [
    module: 関数IRを所有するmodule
    function: 追加するreturn terminatorとname span
  ]
  戻り値: [
    result: 追加・重複・allocation失敗の区別
  ]
}
*/
FcIRAddResult fc_ir_add_function(FcIRModule *module,
                                 FcIRFunction function);

/*
{
  責務: [
    fc_ir_free: moduleが所有する関数配列を解放する
  ]
  処理: [
    1: 関数配列を解放する
    2: moduleを空状態へ戻す
  ]
  引数: [
    module: 解放対象IR
  ]
}
*/
void fc_ir_free(FcIRModule *module);

#endif
