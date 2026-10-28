#include "compiler.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "test failed at line %d: %s\n", __LINE__,          \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

/*
{
  "責務": {"parse_source": "ソースを解析してIRモジュールを返す"},
  "処理": ["トークン列を作る", "IRへ解析する", "トークン列を解放する"],
  "引数": {"source": "検証するソース", "module": "初期化済みIR", "diagnostic": "診断の出力先"},
  "戻り値": ["字句解析と構文解析が完了したら真"]
}
*/
static int parse_source(const char *source, FcIRModule *module,
                        FcDiagnostic *diagnostic) {
    FcTokenList tokens = {0};
    if (!fc_lex_source(source, &tokens, diagnostic)) {
        return 0;
    }
    int success =
        fc_parse_module(tokens.items, tokens.count, module, diagnostic);
    fc_free_tokens(&tokens);
    return success;
}

/*
{
  "責務": {"verify_many_function_names": "関数名表の拡張と重複判定を検証する"},
  "処理": ["多数の異なる名前を追加する", "拡張後に重複を確認する"],
  "戻り値": ["順序と重複判定が正しければ真"]
}
*/
static int verify_many_function_names(void) {
    enum { count = 2048 };
    char names[count][32];
    FcIRModule module = {0};
    int success = 1;
    for (size_t index = 0; index < count; index++) {
        int length =
            snprintf(names[index], sizeof(names[index]), "function_%zu", index);
        FcIRFunction function = {names[index], (size_t)length,
                                 FC_IR_RETURN_VOID, 0};
        if (length < 0 || (size_t)length >= sizeof(names[index]) ||
            fc_ir_add_function(&module, function) != FC_IR_ADD_SUCCESS) {
            success = 0;
            break;
        }
    }
    for (size_t index = 0; success && index < count; index++) {
        FcIRFunction function = {names[index], strlen(names[index]),
                                 FC_IR_RETURN_VOID, 0};
        success =
            module.functions[index].name == names[index] &&
            fc_ir_add_function(&module, function) == FC_IR_ADD_DUPLICATE_NAME;
    }
    success = success && module.count == count;
    fc_ir_free(&module);
    return success;
}

/*
{
  "責務": {"verify_object": "出力ELFのセクション、シンボル、機械語を検証する"},
  "処理": ["ソースをコンパイルしてELFを読む", "シンボルと機械語を照合する"],
  "引数": {"input": "ソースのパス", "output": "出力パス", "expected_names": "関数名の配列", "expected_count": "関数数"},
  "戻り値": ["全ELF条件を満たせば真"]
}
*/
static int verify_object(const char *input, const char *output,
                         const char *const *expected_names,
                         size_t expected_count) {
    FcDiagnostic diagnostic;
    if (fc_compile_file(input, output, &diagnostic) != FC_SUCCESS) {
        fprintf(stderr, "compile failed: %s\n", diagnostic.message);
        return 0;
    }

    FILE *object = fopen(output, "rb");
    if (object == NULL) {
        return 0;
    }
    Elf64_Ehdr header;
    Elf64_Shdr sections[5] = {{0}};
    int valid = fread(&header, 1, sizeof(header), object) == sizeof(header) &&
                fseek(object, (long)header.e_shoff, SEEK_SET) == 0 &&
                fread(sections, sizeof(sections[0]), 5, object) == 5;
    size_t symbol_count = expected_count + 1;
    Elf64_Sym *symbols = calloc(symbol_count, sizeof(*symbols));
    if (symbols == NULL) {
        fclose(object);
        return 0;
    }
    if (valid) {
        valid = sections[2].sh_size == symbol_count * sizeof(*symbols) &&
                fseek(object, (long)sections[2].sh_offset, SEEK_SET) == 0 &&
                fread(symbols, sizeof(*symbols), symbol_count, object) ==
                    symbol_count;
    }
    const unsigned char expected_code[] = {0x31, 0xc0, 0xc3};
    for (size_t index = 0; valid && index < expected_count; index++) {
        char actual_name[256] = {0};
        Elf64_Sym *symbol = &symbols[index + 1];
        valid = symbol->st_name < sections[3].sh_size &&
                fseek(object, (long)(sections[3].sh_offset + symbol->st_name),
                      SEEK_SET) == 0;
        if (valid) {
            size_t remaining = (size_t)(sections[3].sh_size - symbol->st_name);
            size_t name_capacity = remaining < sizeof(actual_name)
                                       ? remaining
                                       : sizeof(actual_name);
            valid = name_capacity > 0 &&
                    fread(actual_name, 1, name_capacity, object) > 0 &&
                    strcmp(actual_name, expected_names[index]) == 0;
        }
        valid = valid &&
                symbol->st_info == ELF64_ST_INFO(STB_GLOBAL, STT_FUNC) &&
                symbol->st_shndx == 1 && symbol->st_value == index * 16 &&
                symbol->st_size == sizeof(expected_code);
        if (valid &&
            fseek(object, (long)(sections[1].sh_offset + symbol->st_value),
                  SEEK_SET) == 0) {
            unsigned char actual_code[sizeof(expected_code)] = {0};
            valid =
                fread(actual_code, 1, sizeof(actual_code), object) ==
                    sizeof(actual_code) &&
                memcmp(actual_code, expected_code, sizeof(expected_code)) == 0;
        } else {
            valid = 0;
        }
    }
    if (valid) {
        valid = sections[1].sh_size ==
                    (expected_count == 0 ? 0 : (expected_count - 1) * 16 + 3) &&
                memcmp(header.e_ident, ELFMAG, SELFMAG) == 0 &&
                header.e_ident[EI_CLASS] == ELFCLASS64 &&
                header.e_ident[EI_DATA] == ELFDATA2LSB &&
                header.e_type == ET_REL && header.e_machine == EM_X86_64 &&
                header.e_shnum == 5 && sections[1].sh_type == SHT_PROGBITS &&
                sections[2].sh_type == SHT_SYMTAB;
    }
    free(symbols);
    fclose(object);
    return valid;
}

/*
{
  "責務": {"main": "構文解析とELF出力のテストを実行する"},
  "処理": ["関数名と構文を確認する", "ELF出力と入力エラーを確認する"],
  "戻り値": ["全テストが通れば0"]
}
*/
int main(void) {
    FcDiagnostic diagnostic = {0};
    FcIRModule module = {0};
    CHECK(verify_many_function_names());
    CHECK(fc_ir_add_function(NULL, (FcIRFunction){0}) ==
          FC_IR_ADD_INVALID_FUNCTION);
    CHECK(parse_source("", &module, &diagnostic));
    CHECK(module.count == 0);
    fc_ir_free(&module);

    CHECK(parse_source("fn work(void) { } fn work2() -> void { } "
                       "fn main(void) -> int { return 0; }",
                       &module, &diagnostic));
    CHECK(module.count == 3);
    CHECK(module.functions[0].name_length == 4 &&
          memcmp(module.functions[0].name, "work", 4) == 0 &&
          module.functions[0].terminator == FC_IR_RETURN_VOID);
    CHECK(module.functions[1].name_length == 5 &&
          memcmp(module.functions[1].name, "work2", 5) == 0 &&
          module.functions[1].terminator == FC_IR_RETURN_VOID);
    CHECK(module.functions[2].name_length == 4 &&
          memcmp(module.functions[2].name, "main", 4) == 0 &&
          module.functions[2].terminator == FC_IR_RETURN_CONSTANT &&
          module.functions[2].return_value == 0);
    fc_ir_free(&module);

    CHECK(!parse_source("fn duplicate(void) { } fn duplicate() { }", &module,
                        &diagnostic));
    CHECK(strcmp(diagnostic.message, "duplicate function name") == 0);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> void { }", &module, &diagnostic));
    CHECK(strcmp(diagnostic.message,
                 "main must use one of the specified entry signatures") == 0);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> int { return 1; }", &module,
                        &diagnostic));
    CHECK(diagnostic.line == 1 && diagnostic.column == 31);
    fc_ir_free(&module);

    FcTokenList invalid_tokens = {0};
    CHECK(!fc_lex_source("fn main(void) { @ }", &invalid_tokens, &diagnostic));
    CHECK(diagnostic.line == 1 && diagnostic.column == 17);

    const char *main_names[] = {"main"};
    const char *multiple_names[] = {"work", "main", "finish"};
    CHECK(verify_object("empty.fscm", "empty.o", NULL, 0));
    CHECK(verify_object("main-void.fscm", "main-void.o", main_names, 1));
    CHECK(verify_object("main-int.fscm", "main-int.o", main_names, 1));
    CHECK(verify_object("multiple-functions.fscm", "multiple-functions.o",
                        multiple_names, 3));
    CHECK(fc_compile_file("missing.fscm", "missing.o", &diagnostic) ==
          FC_INPUT_ERROR);
    return 0;
}
