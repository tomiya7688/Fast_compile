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
  "責務": {"verify_input_diagnostic": "不正な入力の診断パスを検証する"},
  "処理": ["一時ソースを書く", "コンパイルし診断を照合する", "一時ソースを削除する"],
  "引数": {"bytes": "入力バイト列", "length": "入力バイト数", "message": "期待する診断"},
  "戻り値": ["診断が期待に一致すれば真"]
}
*/
static int verify_input_diagnostic(const void *bytes, size_t length,
                                   const char *message) {
    const char *path = "invalid-diagnostic.fscm";
    FILE *source = fopen(path, "wbx");
    if (source == NULL) {
        return 0;
    }
    int wrote = fwrite(bytes, 1, length, source) == length;
    int closed = fclose(source) == 0;
    FcDiagnostic diagnostic = {0};
    int valid = wrote && closed &&
                fc_compile_file(path, "invalid-diagnostic.o", &diagnostic) ==
                    FC_SYNTAX_ERROR &&
                diagnostic.path != NULL && strcmp(diagnostic.path, path) == 0 &&
                strcmp(diagnostic.message, message) == 0;
    remove(path);
    return valid;
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
    size_t first_global = 1;
    for (size_t index = 0; index < expected_count; index++) {
        if (strcmp(expected_names[index], "main") != 0) {
            first_global++;
        }
    }
    Elf64_Sym *symbols = calloc(symbol_count, sizeof(*symbols));
    if (symbols == NULL) {
        fclose(object);
        return 0;
    }
    if (valid) {
        valid = sections[2].sh_size == symbol_count * sizeof(*symbols) &&
                sections[2].sh_info == first_global &&
                fseek(object, (long)sections[2].sh_offset, SEEK_SET) == 0 &&
                fread(symbols, sizeof(*symbols), symbol_count, object) ==
                    symbol_count;
    }
    const unsigned char expected_code[] = {0x31, 0xc0, 0xc3};
    size_t local_index = 1;
    size_t global_index = first_global;
    for (size_t index = 0; valid && index < expected_count; index++) {
        char actual_name[256] = {0};
        int is_entry = strcmp(expected_names[index], "main") == 0;
        Elf64_Sym *symbol = &symbols[is_entry ? global_index++ : local_index++];
        valid = symbol->st_name < sections[3].sh_size &&
                fseek(object, (long)(sections[3].sh_offset + symbol->st_name),
                      SEEK_SET) == 0;
        if (valid) {
            size_t remaining = (size_t)(sections[3].sh_size - symbol->st_name);
            size_t name_capacity = remaining < sizeof(actual_name)
                                       ? remaining
                                       : sizeof(actual_name);
            valid =
                name_capacity > 0 &&
                fread(actual_name, 1, name_capacity, object) == name_capacity &&
                memchr(actual_name, '\0', name_capacity) != NULL &&
                strcmp(actual_name, expected_names[index]) == 0;
        }
        valid =
            valid &&
            symbol->st_info ==
                ELF64_ST_INFO(is_entry ? STB_GLOBAL : STB_LOCAL, STT_FUNC) &&
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
  "責務": {"range_in_file": "バイト範囲がファイル内に収まるか確認する"},
  "処理": ["開始位置と長さをファイルサイズと比較する"],
  "引数": {"offset": "開始位置", "length": "バイト数", "file_size": "ファイルサイズ"},
  "戻り値": ["範囲内なら真"]
}
*/
static int range_in_file(uint64_t offset, uint64_t length, uint64_t file_size) {
    return offset <= file_size && length <= file_size - offset;
}

/*
{
  "責務": {"verify_integer_object": "整数戻り値のELF機械語とシンボルを検証する"},
  "処理": ["ソースをコンパイルする", "シンボル位置、長さ、機械語を照合する"],
  "引数": {"input": "ソースパス", "instructions": "期待する機械語", "sizes": "機械語のバイト数", "symbol_indices": "ソース順のシンボル番号", "count": "関数数"},
  "戻り値": ["全て一致すれば真"]
}
*/
static int verify_integer_object(const char *input,
                                 const unsigned char instructions[][6],
                                 const size_t *sizes,
                                 const size_t *symbol_indices, size_t count) {
    FcDiagnostic diagnostic = {0};
    if (fc_compile_file(input, "integer-return.o", &diagnostic) != FC_SUCCESS) {
        return 0;
    }
    FILE *object = fopen("integer-return.o", "rb");
    if (object == NULL) {
        return 0;
    }
    Elf64_Ehdr header;
    Elf64_Shdr sections[5];
    int valid = count > 0 && fseek(object, 0, SEEK_END) == 0;
    long file_size = valid ? ftell(object) : -1;
    valid =
        valid && file_size >= 0 && fseek(object, 0, SEEK_SET) == 0 &&
        fread(&header, 1, sizeof(header), object) == sizeof(header) &&
        memcmp(header.e_ident, ELFMAG, SELFMAG) == 0 &&
        header.e_ident[EI_CLASS] == ELFCLASS64 &&
        header.e_ident[EI_DATA] == ELFDATA2LSB && header.e_type == ET_REL &&
        header.e_machine == EM_X86_64 && header.e_shnum == 5 &&
        header.e_shstrndx == 4 && header.e_shentsize == sizeof(Elf64_Shdr) &&
        range_in_file(header.e_shoff, sizeof(sections), (uint64_t)file_size) &&
        fseek(object, (long)header.e_shoff, SEEK_SET) == 0 &&
        fread(sections, sizeof(sections[0]), 5, object) == 5 &&
        sections[1].sh_type == SHT_PROGBITS &&
        sections[2].sh_type == SHT_SYMTAB &&
        sections[4].sh_type == SHT_STRTAB &&
        sections[2].sh_entsize == sizeof(Elf64_Sym) &&
        sections[2].sh_size == (count + 1) * sizeof(Elf64_Sym) &&
        range_in_file(sections[1].sh_offset, sections[1].sh_size,
                      (uint64_t)file_size) &&
        range_in_file(sections[2].sh_offset, sections[2].sh_size,
                      (uint64_t)file_size) &&
        range_in_file(sections[4].sh_offset, sections[4].sh_size,
                      (uint64_t)file_size) &&
        sections[1].sh_size == (count - 1) * 16 + sizes[count - 1];
    for (size_t index = 0; valid && index < count; index++) {
        Elf64_Sym symbol;
        unsigned char actual[6] = {0};
        valid =
            sizes[index] <= sizeof(actual) &&
            symbol_indices[index] < count + 1 &&
            fseek(object,
                  (long)(sections[2].sh_offset +
                         symbol_indices[index] * sizeof(symbol)),
                  SEEK_SET) == 0 &&
            fread(&symbol, sizeof(symbol), 1, object) == 1 &&
            symbol.st_value == index * 16 && symbol.st_size == sizes[index] &&
            range_in_file(symbol.st_value, sizes[index], sections[1].sh_size) &&
            fseek(object, (long)(sections[1].sh_offset + symbol.st_value),
                  SEEK_SET) == 0 &&
            fread(actual, 1, sizes[index], object) == sizes[index] &&
            memcmp(actual, instructions[index], sizes[index]) == 0;
    }
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
    const char nul_source[] = {'f', 'n', '\0', 'x'};
    CHECK(verify_input_diagnostic(nul_source, sizeof(nul_source),
                                  "source contains a NUL byte"));
    CHECK(verify_input_diagnostic("@", 1, "invalid source character"));
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
    CHECK(!parse_source("fn main(void) { } fn main() -> int { return 0; }",
                        &module, &diagnostic));
    CHECK(strcmp(diagnostic.message, "duplicate function name") == 0);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> void { }", &module, &diagnostic));
    CHECK(strcmp(diagnostic.message,
                 "main must use one of the specified entry signatures") == 0);
    fc_ir_free(&module);
    CHECK(parse_source("fn main(void) -> int { return 2147483647; }", &module,
                       &diagnostic));
    CHECK(module.count == 1 && module.functions[0].return_value == INT32_MAX);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> int { return 2147483648; }", &module,
                        &diagnostic));
    CHECK(diagnostic.line == 1 && diagnostic.column == 31);
    CHECK(strcmp(diagnostic.message, "integer literal exceeds int range") == 0);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> int { return 4294967296; }", &module,
                        &diagnostic));
    CHECK(strcmp(diagnostic.message, "integer literal exceeds int range") == 0);
    fc_ir_free(&module);
    CHECK(parse_source("fn main(void) -> int { return -2147483648; }", &module,
                       &diagnostic));
    CHECK(module.count == 1 && module.functions[0].return_value == INT32_MIN);
    fc_ir_free(&module);
    CHECK(parse_source("fn main(void) -> int { return - 1; }", &module,
                       &diagnostic));
    CHECK(module.count == 1 && module.functions[0].return_value == -1);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> int { return -2147483649; }", &module,
                        &diagnostic));
    CHECK(diagnostic.line == 1 && diagnostic.column == 32);
    CHECK(strcmp(diagnostic.message, "integer literal exceeds int range") == 0);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> int { return -; }", &module,
                        &diagnostic));
    CHECK(strcmp(diagnostic.message, "expected integer literal") == 0);
    fc_ir_free(&module);
    CHECK(!parse_source("fn main(void) -> int { return --1; }", &module,
                        &diagnostic));
    CHECK(strcmp(diagnostic.message, "expected integer literal") == 0);
    fc_ir_free(&module);
    CHECK(!parse_source(
        "fn main(void) -> int { return 999999999999999999999999; }", &module,
        &diagnostic));
    CHECK(strcmp(diagnostic.message, "integer literal exceeds int range") == 0);
    fc_ir_free(&module);

    FcTokenList invalid_tokens = {0};
    CHECK(!fc_lex_source("fn main(void) { @ }", &invalid_tokens, &diagnostic));
    CHECK(diagnostic.line == 1 && diagnostic.column == 17);

    const char *main_names[] = {"main"};
    const char *multiple_names[] = {"work", "main", "finish"};
    const char *private_names[] = {"work", "finish"};
    const char *main_first_names[] = {"main", "work"};
    CHECK(verify_object("empty.fscm", "empty.o", NULL, 0));
    CHECK(verify_object("main-void.fscm", "main-void.o", main_names, 1));
    CHECK(verify_object("main-int.fscm", "main-int.o", main_names, 1));
    CHECK(verify_object("multiple-functions.fscm", "multiple-functions.o",
                        multiple_names, 3));
    CHECK(
        verify_object("private-only.fscm", "private-only.o", private_names, 2));
    CHECK(
        verify_object("main-first.fscm", "main-first.o", main_first_names, 2));
    const unsigned char integer_code[][6] = {
        {0xb8, 0x01, 0, 0, 0, 0xc3},
        {0xb8, 0x2a, 0, 0, 0, 0xc3},
        {0xb8, 0xff, 0xff, 0xff, 0x7f, 0xc3}};
    const size_t integer_sizes[] = {6, 6, 6};
    const size_t integer_symbols[] = {1, 3, 2};
    CHECK(verify_integer_object("constant-returns.fscm", integer_code,
                                integer_sizes, integer_symbols, 3));
    const unsigned char negative_code[][6] = {
        {0xb8, 0xff, 0xff, 0xff, 0xff, 0xc3},
        {0xb8, 0x00, 0x00, 0x00, 0x80, 0xc3},
        {0x31, 0xc0, 0xc3}};
    const size_t negative_sizes[] = {6, 6, 3};
    const size_t negative_symbols[] = {1, 3, 2};
    CHECK(verify_integer_object("negative-returns.fscm", negative_code,
                                negative_sizes, negative_symbols, 3));
    const unsigned char mixed_code[][6] = {{0xb8, 0x2a, 0, 0, 0, 0xc3},
                                           {0x31, 0xc0, 0xc3}};
    const size_t mixed_sizes[] = {6, 3};
    const size_t mixed_symbols[] = {1, 2};
    CHECK(verify_integer_object("mixed-returns.fscm", mixed_code, mixed_sizes,
                                mixed_symbols, 2));
    CHECK(fc_compile_file("missing.fscm", "missing.o", &diagnostic) ==
          FC_INPUT_ERROR);
    return 0;
}
