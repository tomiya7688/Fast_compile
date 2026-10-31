#include "ir.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
{
  "責務": {"hash_name": "関数名から探索用のハッシュを作る"},
  "処理": ["全バイトを順にハッシュへ混ぜる"],
  "引数": {"name": "関数名の先頭", "length": "関数名のバイト数"},
  "戻り値": ["関数名のハッシュ値"]
}
*/
static uint64_t hash_name(const char *name, size_t length) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < length; index++) {
        hash ^= (unsigned char)name[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

/*
{
  "責務": {"find_name_slot": "関数名が存在する位置または空位置を探す"},
  "処理": ["ハッシュ位置から空欄または一致する名前まで探索する"],
  "引数": {"module": "探索対象", "function": "探す関数名"},
  "戻り値": ["関数名に対応する位置"],
  "補足": ["探索表には必ず空きがあることを呼び出し側が保証する"]
}
*/
static size_t find_name_slot(const FcIRModule *module, FcIRFunction function) {
    size_t slot = (size_t)hash_name(function.name, function.name_length) &
                  (module->name_capacity - 1);
    while (module->name_slots[slot] != 0) {
        const FcIRFunction *existing =
            &module->functions[module->name_slots[slot] - 1];
        if (existing->name_length == function.name_length &&
            memcmp(existing->name, function.name, function.name_length) == 0) {
            break;
        }
        slot = (slot + 1) & (module->name_capacity - 1);
    }
    return slot;
}

/*
{
  "責務": {"grow_name_table": "関数名の探索表を拡張して既存名を再配置する"},
  "処理": ["サイズの範囲を確認して新しい表を確保する", "既存名を再配置する"],
  "引数": {"module": "関数名と探索表を所有するモジュール"},
  "戻り値": ["表を拡張できたら真"]
}
*/
static int grow_name_table(FcIRModule *module) {
    size_t capacity =
        module->name_capacity == 0 ? 16 : module->name_capacity * 2;
    if (capacity < module->name_capacity ||
        capacity > SIZE_MAX / sizeof(size_t)) {
        return 0;
    }
    size_t *slots = calloc(capacity, sizeof(*slots));
    if (slots == NULL) {
        return 0;
    }
    free(module->name_slots);
    module->name_slots = slots;
    module->name_capacity = capacity;
    for (size_t index = 0; index < module->count; index++) {
        size_t slot = find_name_slot(module, module->functions[index]);
        module->name_slots[slot] = index + 1;
    }
    return 1;
}

/*
{
  "責務": {"fc_ir_add_function": "重複名を拒否して関数IRを追加する"},
  "処理": ["名前の探索表で重複を確認する", "必要な配列を拡張する", "関数を追加する"],
  "引数": {"module": "配列を所有するIRモジュール", "function": "追加する関数IR"},
  "戻り値": ["FC_IR_ADD_SUCCESS、FC_IR_ADD_DUPLICATE_NAME、FC_IR_ADD_INVALID_FUNCTION、FC_IR_ADD_ALLOCATION_ERRORのいずれか"]
}
*/
FcIRAddResult fc_ir_add_function(FcIRModule *module, FcIRFunction function) {
    if (module == NULL || function.name == NULL || function.name_length == 0 ||
        (module->count > 0 && module->functions == NULL)) {
        return FC_IR_ADD_INVALID_FUNCTION;
    }
    if (module->name_capacity == 0) {
        if (!grow_name_table(module)) {
            return FC_IR_ADD_ALLOCATION_ERROR;
        }
    }
    size_t slot = find_name_slot(module, function);
    if (module->name_slots[slot] != 0) {
        return FC_IR_ADD_DUPLICATE_NAME;
    }
    if (module->count >= module->name_capacity / 2) {
        if (!grow_name_table(module)) {
            return FC_IR_ADD_ALLOCATION_ERROR;
        }
        slot = find_name_slot(module, function);
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

    module->functions[module->count] = function;
    module->name_slots[slot] = ++module->count;
    return FC_IR_ADD_SUCCESS;
}

/*
{
  "責務": {"fc_ir_free": "モジュールの関数配列と探索表を解放する"},
  "処理": ["所有する配列を解放する", "ポインターと要素数を初期化する"],
  "引数": {"module": "解放対象IRモジュール"}
}
*/
void fc_ir_free(FcIRModule *module) {
    if (module == NULL) {
        return;
    }
    free(module->functions);
    free(module->name_slots);
    module->functions = NULL;
    module->count = 0;
    module->capacity = 0;
    module->name_slots = NULL;
    module->name_capacity = 0;
}
