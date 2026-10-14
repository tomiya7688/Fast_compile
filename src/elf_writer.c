#include "elf_writer.h"

#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char return_zero_code[] = {0x31, 0xc0, 0xc3};
static const char section_strings[] = "\0.text\0.symtab\0.strtab\0.shstrtab\0";

/*
{
  責務: [
    write_bytes: 指定したバイト列をファイルへ完全に書き出す
  ]
  処理: [
    1: fwriteで全バイトを書き込む
    2: 書き込み不足を失敗として返す
  ]
  引数: [
    output: 出力ファイル
    bytes: 書き込む先頭アドレス
    length: 書き込むバイト数
  ]
  戻り値: [
    success: 指定サイズ全てを書けたら真
  ]
}
*/
static int write_bytes(FILE *output, const void *bytes, size_t length) {
    return length == 0 || fwrite(bytes, 1, length, output) == length;
}

/*
{
  責務: [
    pad_to: 指定offsetまでELF section alignment用のゼロを出力する
  ]
  処理: [
    1: 現在位置が目標より後ろなら失敗する
    2: 必要な数だけゼロを書き出す
  ]
  引数: [
    output: 出力ファイル
    current: 現在のファイルoffset
    target: 次sectionの開始offset
  ]
  戻り値: [
    success: 目標位置へ移動できたら真
  ]
}
*/
static int pad_to(FILE *output, uint64_t current, uint64_t target) {
    static const unsigned char zeros[16] = {0};
    if (target < current) {
        return 0;
    }
    while (current < target) {
        size_t count = target - current > sizeof(zeros)
                           ? sizeof(zeros)
                           : (size_t)(target - current);
        if (!write_bytes(output, zeros, count)) {
            return 0;
        }
        current += count;
    }
    return 1;
}

/*
{
  責務: [
    build_function_tables: module IRからELF symbol table・name table・textを作る
  ]
  処理: [
    1: 関数数・name span・offset計算のoverflowを検証する
    2: 関数nameをstring tableへsource順に複写する
    3: function symbolとreturn-zero machine codeを設定する
  ]
  引数: [
    module: source name spanを保持する関数IR列
    symbols: ELF64 symbol tableの出力先
    symbol_names: ELF string tableの出力先
    symbol_names_size: string table byte数
    text: function codeとalignment paddingの出力先
    text_size: text section byte数
  ]
  戻り値: [
    success: ELF tableを構築できたら真
  ]
}
*/
static int build_function_tables(const FcIRModule *module, Elf64_Sym *symbols,
                                 char *symbol_names,
                                 size_t symbol_names_size,
                                 unsigned char *text, size_t text_size) {
    size_t name_offset = 1;
    symbol_names[0] = '\0';
    for (size_t index = 0; index < module->count; index++) {
        const FcIRFunction *function = &module->functions[index];
        size_t code_offset = index * 16;
        if (function->name == NULL || function->name_length == 0 ||
            name_offset >= symbol_names_size ||
            function->name_length > symbol_names_size - name_offset - 1 ||
            code_offset > text_size ||
            sizeof(return_zero_code) > text_size - code_offset ||
            (function->terminator != FC_IR_RETURN_VOID &&
             (function->terminator != FC_IR_RETURN_CONSTANT ||
              function->return_value != 0))) {
            return 0;
        }

        memcpy(symbol_names + name_offset, function->name,
               function->name_length);
        symbol_names[name_offset + function->name_length] = '\0';
        symbols[index + 1].st_name = (Elf64_Word)name_offset;
        symbols[index + 1].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
        symbols[index + 1].st_shndx = 1;
        symbols[index + 1].st_value = code_offset;
        symbols[index + 1].st_size = sizeof(return_zero_code);
        memcpy(text + code_offset, return_zero_code, sizeof(return_zero_code));
        name_offset += function->name_length + 1;
    }
    return name_offset == symbol_names_size;
}

/*
{
  責務: [
    fc_write_elf_object: 関数IR列をLinux x86-64 ELF64 relocatable objectへ変換する
  ]
  処理: [
    1: function数に応じたtext・symbol・string tableを確保する
    2: ELF headerと5つのsection headerを作る
    3: 各sectionをalignmentに従って指定pathへ書き出す
  ]
  引数: [
    path: ELF objectの出力先
    module: source順で並んだ関数IR列
  ]
  戻り値: [
    success: ELF objectを書き終えたら真
  ]
}
*/
int fc_write_elf_object(const char *path, const FcIRModule *module) {
    if (path == NULL || module == NULL ||
        (module->count > 0 && module->functions == NULL) ||
        module->count > UINT32_MAX - 1 ||
        module->count > (SIZE_MAX - sizeof(return_zero_code)) / 16 + 1) {
        return 0;
    }

    size_t symbol_names_size = 1;
    for (size_t index = 0; index < module->count; index++) {
        size_t name_length = module->functions[index].name_length;
        if (name_length == 0 || symbol_names_size >= UINT32_MAX ||
            name_length > UINT32_MAX - symbol_names_size - 1) {
            return 0;
        }
        symbol_names_size += name_length + 1;
    }

    size_t symbol_count = module->count + 1;
    size_t text_size = module->count == 0
                           ? 0
                           : (module->count - 1) * 16 + sizeof(return_zero_code);
    if (symbol_count > SIZE_MAX / sizeof(Elf64_Sym)) {
        return 0;
    }
    Elf64_Sym *symbols = calloc(symbol_count, sizeof(Elf64_Sym));
    char *symbol_names = malloc(symbol_names_size);
    unsigned char *text = calloc(text_size == 0 ? 1 : text_size, 1);
    if (symbols == NULL || symbol_names == NULL || text == NULL) {
        free(symbols);
        free(symbol_names);
        free(text);
        return 0;
    }
    if (!build_function_tables(module, symbols, symbol_names, symbol_names_size,
                               text, text_size)) {
        free(symbols);
        free(symbol_names);
        free(text);
        return 0;
    }

    const uint64_t text_offset = sizeof(Elf64_Ehdr);
    const uint64_t symtab_offset = (text_offset + text_size + 7) & ~UINT64_C(7);
    const uint64_t symtab_size = symbol_count * sizeof(Elf64_Sym);
    const uint64_t strtab_offset = symtab_offset + symtab_size;
    const uint64_t shstrtab_offset = strtab_offset + symbol_names_size;
    const uint64_t sections_offset =
        (shstrtab_offset + sizeof(section_strings) - 1 + 7) & ~UINT64_C(7);

    Elf64_Ehdr header = {0};
    memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_ident[EI_OSABI] = ELFOSABI_NONE;
    header.e_type = ET_REL;
    header.e_machine = EM_X86_64;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(Elf64_Ehdr);
    header.e_shoff = sections_offset;
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_shnum = 5;
    header.e_shstrndx = 4;

    Elf64_Shdr sections[5] = {{0}};
    sections[1].sh_name = 1;
    sections[1].sh_type = SHT_PROGBITS;
    sections[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    sections[1].sh_offset = text_offset;
    sections[1].sh_size = text_size;
    sections[1].sh_addralign = 16;
    sections[2].sh_name = 7;
    sections[2].sh_type = SHT_SYMTAB;
    sections[2].sh_offset = symtab_offset;
    sections[2].sh_size = symtab_size;
    sections[2].sh_link = 3;
    sections[2].sh_info = 1;
    sections[2].sh_addralign = 8;
    sections[2].sh_entsize = sizeof(Elf64_Sym);
    sections[3].sh_name = 15;
    sections[3].sh_type = SHT_STRTAB;
    sections[3].sh_offset = strtab_offset;
    sections[3].sh_size = symbol_names_size;
    sections[3].sh_addralign = 1;
    sections[4].sh_name = 23;
    sections[4].sh_type = SHT_STRTAB;
    sections[4].sh_offset = shstrtab_offset;
    sections[4].sh_size = sizeof(section_strings) - 1;
    sections[4].sh_addralign = 1;

    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        free(symbols);
        free(symbol_names);
        free(text);
        return 0;
    }

    int success = write_bytes(output, &header, sizeof(header)) &&
                  write_bytes(output, text, text_size) &&
                  pad_to(output, text_offset + text_size, symtab_offset) &&
                  write_bytes(output, symbols, (size_t)symtab_size) &&
                  write_bytes(output, symbol_names, symbol_names_size) &&
                  write_bytes(output, section_strings, sizeof(section_strings) - 1) &&
                  pad_to(output, shstrtab_offset + sizeof(section_strings) - 1,
                         sections_offset) &&
                  write_bytes(output, sections, sizeof(sections));
    if (fclose(output) != 0) {
        success = 0;
    }
    free(symbols);
    free(symbol_names);
    free(text);
    return success;
}
