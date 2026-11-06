#ifndef FASTCOMPILE_IR_H
#define FASTCOMPILE_IR_H

#include <stddef.h>
#include <stdint.h>

typedef enum { FC_IR_RETURN_VOID, FC_IR_RETURN_CONSTANT } FcIRTerminator;

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
    size_t *name_slots;
    size_t name_capacity;
} FcIRModule;

typedef enum {
    FC_IR_ADD_SUCCESS,
    FC_IR_ADD_DUPLICATE_NAME,
    FC_IR_ADD_INVALID_FUNCTION,
    FC_IR_ADD_ALLOCATION_ERROR
} FcIRAddResult;

/*
{
  "責務": {"fc_ir_add_function": "関数IRをモジュールに追加する"},
  "処理": ["関数名の重複を確認する", "必要な配列を拡張し、ソース順に追加する"],
  "引数": {"module": "関数IRを所有するモジュール", "function": "追加する関数IR"},
  "戻り値": ["FC_IR_ADD_SUCCESS、FC_IR_ADD_DUPLICATE_NAME、FC_IR_ADD_INVALID_FUNCTION、FC_IR_ADD_ALLOCATION_ERRORのいずれか"],
  "補足": ["モジュールはゼロ初期化してAPIからのみ変更する", "関数名は借用するため、モジュール解放まで有効なままにする"]
}
*/
FcIRAddResult fc_ir_add_function(FcIRModule *module, FcIRFunction function);

/*
{
  "責務": {"fc_ir_free": "モジュールが所有する配列を解放する"},
  "処理": ["関数配列と名前の探索表を解放する", "モジュールを空状態へ戻す"],
  "引数": {"module": "解放対象IR"}
}
*/
void fc_ir_free(FcIRModule *module);

#endif
