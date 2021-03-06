#include "defines.h"
#include "elf.h"
#include "lib.h"

struct elf_header {
  struct {
    unsigned char magic[4];
    unsigned char class;
    unsigned char format;
    unsigned char version;
    unsigned char abi;
    unsigned char abi_version;
    unsigned char reserve[7];
  } id;
  short type;
  short arch;
  long version;
  long entry_point;
  long program_header_offset;
  long section_header_offset;
  long flags;
  short header_size;
  short program_header_size;
  short program_header_num;
  short section_header_size;
  short section_header_num;
  short section_name_index;
};

struct elf_program_header {
  long type;
  long offset;
  long virtual_addr;
  long physical_addr;
  long file_size;
  long memory_size;
  long flags;
  long align;
};

// ELFヘッダのバリデーション
static int elf_check(struct elf_header *header) {
  if (memcmp(header->id.magic, "\x7f" "ELF", 4)) // 0x7f 'E' 'L' 'F'
    return -1;

  if (header->id.class != 1) return -1; // ELF32
  if (header->id.format != 2) return -1; // Big endian
  if (header->id.version != 1) return -1; // Version 1
  if (header->type != 2) return -1; // Executable file
  if (header->version != 1) return -1; // Version 1

  // Hitachi H8/300 or H8/300H
  if ((header->arch != 46) && (header->arch != 47)) return -1;

  return 0;
}

// プログラムヘッダ情報を出力。
static void print_program_header(struct elf_program_header *phdr) {
  putxval(phdr->offset, 6); puts(" ");
  putxval(phdr->virtual_addr, 8); puts(" ");
  putxval(phdr->physical_addr, 8); puts(" ");
  putxval(phdr->file_size, 5); puts(" ");
  putxval(phdr->memory_size, 5); puts(" ");
  putxval(phdr->flags, 2); puts(" ");
  putxval(phdr->align, 2); puts("\n");
}

// セグメント単位でのロード
static int elf_load_program(struct elf_header *header) {
  int i;
  struct elf_program_header *phdr;

  // セグメント単位でループ
  for (i = 0; i < header->program_header_num; i++) {
    // プログラム・ヘッダを取得
    phdr = (struct elf_program_header *)
      ((char *)header + header->program_header_offset + header->program_header_size * i);
 
    if (phdr->type != 1) // ロード可能なセグメントか
      continue;

    // print_program_header(phdr);
    memcpy((char *)phdr->physical_addr, // ロード先物理アドレス
           (char *)header + phdr->offset, // セグメントのファイル内の位置
           phdr->file_size);
    // .dataセクションと.bssセクションは同じ属性のため、ひとつのセグメントにまとめられている。
    // BSS領域はファイル内では実体を持たないため、メモリ上のサイズに対してファイルサイズが小さくなる。
    memset((char *)phdr->physical_addr + phdr->file_size,
           0, // BSS領域を0で初期化。
           phdr->memory_size - phdr->file_size);
  }
  
  return 0;
}

char *elf_load(char *buf) {
  struct elf_header *header = (struct elf_header *)buf;

  if (elf_check(header) < 0) // ELFヘッダをチェック。
    return NULL;

  if (elf_load_program(header) < 0)
    return NULL;

  return (char *)header->entry_point; // エントリポイントを返す。
}
