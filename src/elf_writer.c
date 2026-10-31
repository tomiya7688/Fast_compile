#include "elf_writer.h"

#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char return_zero_code[] = {0x31, 0xc0, 0xc3};
static const size_t return_integer_code_size = 6;
static const char section_strings[] = "\0.text\0.symtab\0.strtab\0.shstrtab\0";

/*
{
  "責務": {"function_code_size": "対応する関数の機械語の長さを返す"},
  "処理": ["終端の種別とintの範囲を検証する", "ゼロまたは非ゼロのコード長を選ぶ"],
  "引数": {"function": "関数IR"},
  "戻り値": ["対応するコード長、未対応なら0"]
}
*/
static size_t function_code_size(const FcIRFunction *function) {
    if (function->terminator == FC_IR_RETURN_VOID) {
        return sizeof(return_zero_code);
    }
    if (function->terminator != FC_IR_RETURN_CONSTANT ||
        function->return_value < INT32_MIN ||
        function->return_value > INT32_MAX) {
        return 0;
    }
    return function->return_value == 0 ? sizeof(return_zero_code)
                                       : return_integer_code_size;
}

/*
{
  "責務": {"write_function_code": "検証済みの関数IRをx86-64機械語へ変換する"},
  "処理": ["ゼロの戻り値にはゼロ化命令を使う", "非ゼロのintを即値として書く"],
  "引数": {"function": "検証済み関数IR", "output": "十分な大きさの出力先"}
}
*/
static void write_function_code(const FcIRFunction *function,
                                unsigned char *output) {
    if (function->terminator == FC_IR_RETURN_VOID ||
        function->return_value == 0) {
        memcpy(output, return_zero_code, sizeof(return_zero_code));
        return;
    }
    uint32_t value = (uint32_t)function->return_value;
    output[0] = 0xb8;
    for (unsigned index = 0; index < 4; index++) {
        output[index + 1] = (unsigned char)(value >> (index * 8));
    }
    output[5] = 0xc3;
}

/*
{
  "責務": {"write_bytes": "指定したバイト列をファイルへ完全に書き出す"},
  "処理": ["全バイトを書き込み、書き込み不足を検出する"],
  "引数": {"output": "出力ファイル", "bytes": "バイト列", "length": "バイト数"},
  "戻り値": ["指定サイズ全てを書けたら真"]
}
*/
static int write_bytes(FILE *output, const void *bytes, size_t length) {
    return length == 0 || fwrite(bytes, 1, length, output) == length;
}

/*
{
  "責務": {"pad_to": "指定位置までセクション整列用のゼロを出力する"},
  "処理": ["現在位置を確認し、必要な数だけゼロを書き出す"],
  "引数": {"output": "出力ファイル", "current": "現在位置", "target": "目標位置"},
  "戻り値": ["目標位置へ移動できたら真"]
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
  "責務": {"is_entry_function": "関数名がエントリーポイントか判定する"},
  "処理": ["名前の長さとバイト列をmainと比較する"],
  "引数": {"function": "判定する関数IR"},
  "戻り値": ["関数名がmainなら真"]
}
*/
static int is_entry_function(const FcIRFunction *function) {
    return function->name != NULL && function->name_length == 4 &&
           memcmp(function->name, "main", 4) == 0;
}

/*
{
  "責務": {"build_function_tables": "関数IRからELFのシンボルとコードを作る"},
  "処理": ["関数名とコードの範囲を検証する", "ローカルシンボルを大域シンボルより先に配置する", "名前と機械語を格納する"],
  "引数": {"module": "関数IR", "symbols": "シンボル表", "symbol_names": "名前表", "symbol_names_size": "名前表のバイト数", "text": "機械語の出力先", "text_size": "機械語のバイト数", "first_global": "最初の大域シンボルの位置"},
  "戻り値": ["ELF表を構築できたら真"]
}
*/
static int build_function_tables(const FcIRModule *module, Elf64_Sym *symbols,
                                 char *symbol_names, size_t symbol_names_size,
                                 unsigned char *text, size_t text_size,
                                 size_t first_global) {
    size_t name_offset = 1;
    size_t local_index = 1;
    size_t global_index = first_global;
    symbol_names[0] = '\0';
    for (size_t index = 0; index < module->count; index++) {
        const FcIRFunction *function = &module->functions[index];
        size_t code_offset = index * 16;
        size_t code_size = function_code_size(function);
        if (function->name == NULL || function->name_length == 0 ||
            name_offset >= symbol_names_size ||
            function->name_length > symbol_names_size - name_offset - 1 ||
            code_size == 0 || code_offset > text_size ||
            code_size > text_size - code_offset) {
            return 0;
        }

        memcpy(symbol_names + name_offset, function->name,
               function->name_length);
        symbol_names[name_offset + function->name_length] = '\0';
        int is_entry = is_entry_function(function);
        size_t symbol_index = is_entry ? global_index++ : local_index++;
        symbols[symbol_index].st_name = (Elf64_Word)name_offset;
        symbols[symbol_index].st_info =
            ELF64_ST_INFO(is_entry ? STB_GLOBAL : STB_LOCAL, STT_FUNC);
        symbols[symbol_index].st_shndx = 1;
        symbols[symbol_index].st_value = code_offset;
        symbols[symbol_index].st_size = code_size;
        write_function_code(function, text + code_offset);
        name_offset += function->name_length + 1;
    }
    return name_offset == symbol_names_size;
}

/*
{
  "責務": {"fc_write_elf_object": "関数IRをLinux x86-64 ELFオブジェクトへ変換する"},
  "処理": ["出力用の表を確保する", "ELFヘッダーを構築する", "整列して各セクションを書き出す"],
  "引数": {"path": "出力先", "module": "ソース順の関数IR"},
  "戻り値": ["ELFオブジェクトを書き終えたら真"]
}
*/
int fc_write_elf_object(const char *path, const FcIRModule *module) {
    if (path == NULL || module == NULL ||
        (module->count > 0 && module->functions == NULL) ||
        module->count > UINT32_MAX - 1 ||
        module->count > (SIZE_MAX - return_integer_code_size) / 16 + 1) {
        return 0;
    }

    size_t symbol_names_size = 1;
    size_t global_count = 0;
    size_t last_code_size = 0;
    for (size_t index = 0; index < module->count; index++) {
        size_t name_length = module->functions[index].name_length;
        last_code_size = function_code_size(&module->functions[index]);
        if (module->functions[index].name == NULL || name_length == 0 ||
            last_code_size == 0 || symbol_names_size >= UINT32_MAX ||
            name_length > UINT32_MAX - symbol_names_size - 1) {
            return 0;
        }
        global_count += is_entry_function(&module->functions[index]);
        symbol_names_size += name_length + 1;
    }

    size_t symbol_count = module->count + 1;
    size_t first_global = symbol_count - global_count;
    size_t text_size =
        module->count == 0 ? 0 : (module->count - 1) * 16 + last_code_size;
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
                               text, text_size, first_global)) {
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
    sections[2].sh_info = (Elf64_Word)first_global;
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

    int success =
        write_bytes(output, &header, sizeof(header)) &&
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
