#include "ir.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
{
  責務: [
    fc_ir_add_function: 重複名を拒否しmoduleの関数IR配列へ安全に追加する
  ]
  処理: [
    1: borrowed name spanを長さ付きで比較する
    2: size overflowを検査し容量を倍増する
    3: functionを追加し結果コードを返す
  ]
  引数: [
    module: 配列を所有するIR module
    function: parserが作った関数IR
  ]
  戻り値: [
    result: 追加成功・重複・memory確保失敗
  ]
}
*/
FcIRAddResult fc_ir_add_function(FcIRModule *module,
                                 FcIRFunction function) {
    if (module == NULL || function.name == NULL || function.name_length == 0 ||
        (module->count > 0 && module->functions == NULL)) {
        return FC_IR_ADD_INVALID_FUNCTION;
    }
    for (size_t index = 0; index < module->count; index++) {
        const FcIRFunction *existing = &module->functions[index];
        if (existing->name_length == function.name_length &&
            memcmp(existing->name, function.name, function.name_length) == 0) {
            return FC_IR_ADD_DUPLICATE_NAME;
        }
    }

    if (module->count == module->capacity) {
        size_t capacity = module->capacity == 0 ? 8 : module->capacity * 2;
        if (capacity < module->capacity ||
            capacity > SIZE_MAX / sizeof(FcIRFunction)) {
            return FC_IR_ADD_ALLOCATION_ERROR;
        }
        FcIRFunction *functions =
            realloc(module->functions, capacity * sizeof(FcIRFunction));
        if (functions == NULL) {
            return FC_IR_ADD_ALLOCATION_ERROR;
        }
        module->functions = functions;
        module->capacity = capacity;
    }

    module->functions[module->count++] = function;
    return FC_IR_ADD_SUCCESS;
}

/*
{
  責務: [
    fc_ir_free: moduleの関数配列を解放し再利用可能な状態へ戻す
  ]
  処理: [
    1: 関数配列を解放する
    2: pointer・count・capacityを初期化する
  ]
  引数: [
    module: 解放対象IR module
  ]
}
*/
void fc_ir_free(FcIRModule *module) {
    if (module == NULL) {
        return;
    }
    free(module->functions);
    module->functions = NULL;
    module->count = 0;
    module->capacity = 0;
}
