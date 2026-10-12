#include "elf_writer.h"

#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const unsigned char empty_code[] = {0x31, 0xc0, 0xc3};
static const char symbol_strings[] = "\0main\0";
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
    fc_write_elf_object: 空モジュールまたはreturn 0だけのmainをELF64形式へ変換する
  ]
  処理: [
    1: 5 sectionのELF64 relocatable object構造を設定する
    2: sectionをalignmentとoffsetに従って書き出す
    3: 書き込み失敗またはclose失敗を検出する
  ]
  引数: [
    path: object fileの出力先
    kind: 空・void main・int mainの種別
  ]
  戻り値: [
    success: ELF objectを保存できたら真
  ]
}
*/
int fc_write_elf_object(const char *path, FcModuleKind kind) {
    const int has_main = kind != FC_MODULE_EMPTY;
    const size_t code_size = has_main ? sizeof(empty_code) : 0;
    const size_t symbol_count = has_main ? 2 : 1;
    const size_t string_size = has_main ? sizeof(symbol_strings) : 1;
    const uint64_t text_offset = sizeof(Elf64_Ehdr);
    const uint64_t symtab_offset = (text_offset + code_size + 7) & ~UINT64_C(7);
    const uint64_t symtab_size = symbol_count * sizeof(Elf64_Sym);
    const uint64_t strtab_offset = symtab_offset + symtab_size;
    const uint64_t shstrtab_offset = strtab_offset + string_size;
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

    Elf64_Sym symbols[2] = {{0}};
    if (has_main) {
        symbols[1].st_name = 1;
        symbols[1].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
        symbols[1].st_shndx = 1;
        symbols[1].st_size = code_size;
    }

    Elf64_Shdr sections[5] = {{0}};
    sections[1].sh_name = 1;
    sections[1].sh_type = SHT_PROGBITS;
    sections[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    sections[1].sh_offset = text_offset;
    sections[1].sh_size = code_size;
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
    sections[3].sh_size = string_size;
    sections[3].sh_addralign = 1;
    sections[4].sh_name = 23;
    sections[4].sh_type = SHT_STRTAB;
    sections[4].sh_offset = shstrtab_offset;
    sections[4].sh_size = sizeof(section_strings) - 1;
    sections[4].sh_addralign = 1;

    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        return 0;
    }

    int success = write_bytes(output, &header, sizeof(header)) &&
                  write_bytes(output, empty_code, code_size) &&
                  pad_to(output, text_offset + code_size, symtab_offset) &&
                  write_bytes(output, symbols, symtab_size) &&
                  write_bytes(output, has_main ? symbol_strings : "", string_size) &&
                  write_bytes(output, section_strings, sizeof(section_strings) - 1) &&
                  pad_to(output, shstrtab_offset + sizeof(section_strings) - 1,
                         sections_offset) &&
                  write_bytes(output, sections, sizeof(sections));
    if (fclose(output) != 0) {
        success = 0;
    }
    return success;
}
