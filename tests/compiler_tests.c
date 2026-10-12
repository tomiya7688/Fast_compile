#include "compiler.h"
#include "parser.h"

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "test failed at line %d: %s\n", __LINE__,         \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                       \
    } while (0)

/*
{
  責務: [
    verify_object: golden入力のコンパイル結果がELF64 relocatable objectか検証する
  ]
  処理: [
    1: コンパイル結果のELF headerを読む
    2: architectureとobject typeを確認する
    3: section数とmain symbolの有無を検証する
  ]
  引数: [
    input: golden source path
    output: 一時ELF object path
    has_main: main symbol期待値
  ]
  戻り値: [
    success: 全ELF条件を満たせば真
  ]
}
*/
static int verify_object(const char *input, const char *output, int has_main) {
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
    int valid = fread(&header, 1, sizeof(header), object) == sizeof(header);
    Elf64_Shdr sections[5];
    if (valid && fseek(object, (long)header.e_shoff, SEEK_SET) == 0) {
        valid = fread(sections, sizeof(sections[0]), 5, object) == 5;
    } else {
        valid = 0;
    }
    Elf64_Sym symbols[2] = {{0}};
    if (valid && fseek(object, (long)sections[2].sh_offset, SEEK_SET) == 0) {
        size_t count = sections[2].sh_size / sizeof(Elf64_Sym);
        valid = count >= 1 && count <= 2 &&
                fread(symbols, sizeof(symbols[0]), count, object) == count;
        valid = valid && ((count == 2) == has_main);
    } else {
        valid = 0;
    }
    const unsigned char expected_code[] = {0x31, 0xc0, 0xc3};
    unsigned char actual_code[sizeof(expected_code)] = {0};
    if (valid && fseek(object, (long)sections[1].sh_offset, SEEK_SET) == 0) {
        valid = sections[1].sh_size == (has_main ? sizeof(expected_code) : 0);
        if (valid && has_main) {
            valid = fread(actual_code, 1, sizeof(actual_code), object) ==
                        sizeof(actual_code) &&
                    memcmp(actual_code, expected_code, sizeof(expected_code)) ==
                        0;
        }
    } else {
        valid = 0;
    }
    if (valid && has_main &&
        fseek(object, (long)(sections[3].sh_offset + symbols[1].st_name),
              SEEK_SET) == 0) {
        char symbol_name[5] = {0};
        valid = fread(symbol_name, 1, 5, object) == 5 &&
                strcmp(symbol_name, "main") == 0;
    } else if (valid && has_main) {
        valid = 0;
    }
    fclose(object);
    return valid && memcmp(header.e_ident, ELFMAG, SELFMAG) == 0 &&
           header.e_ident[EI_CLASS] == ELFCLASS64 &&
           header.e_ident[EI_DATA] == ELFDATA2LSB &&
           header.e_type == ET_REL && header.e_machine == EM_X86_64 &&
           header.e_shnum == 5 && sections[1].sh_type == SHT_PROGBITS &&
           sections[2].sh_type == SHT_SYMTAB &&
           (!has_main || (symbols[1].st_name == 1 &&
                          ELF64_ST_TYPE(symbols[1].st_info) == STT_FUNC &&
                          symbols[1].st_shndx == 1));
}

/*
{
  責務: [
    main: parser unit casesとELF golden casesを実行する
  ]
  処理: [
    1: 対応するmain構文と不正構文を確認する
    2: 空・void main・int mainのELF objectを生成する
    3: 不正な入力pathの終了状態を確認する
  ]
  戻り値: [
    result: 全testが通れば0
  ]
}
*/
int main(void) {
    FcDiagnostic diagnostic = {0};
    FcModuleKind kind;
    CHECK(fc_parse_module("", &kind, &diagnostic) && kind == FC_MODULE_EMPTY);
    CHECK(fc_parse_module("fn main(void) {}", &kind, &diagnostic) &&
          kind == FC_MAIN_VOID);
    CHECK(fc_parse_module("fn main(void) -> int { return 0; }", &kind,
                          &diagnostic) &&
          kind == FC_MAIN_INT_ZERO);
    CHECK(!fc_parse_module("fn main(void) -> int { return 1; }", &kind,
                           &diagnostic));
    CHECK(verify_object("empty.fscm", "empty.o", 0));
    CHECK(verify_object("main-void.fscm", "main-void.o", 1));
    CHECK(verify_object("main-int.fscm", "main-int.o", 1));
    CHECK(fc_compile_file("missing.fscm", "missing.o", &diagnostic) ==
          FC_INPUT_ERROR);
    return 0;
}
