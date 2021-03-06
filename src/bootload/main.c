#include "defines.h"
#include "interrupt.h"
#include "serial.h"
#include "xmodem.h"
#include "elf.h"
#include "lib.h"

static int init(void) {
  // 以下はリンカ・スクリプトで定義してあるシンボル。
  extern int erodata, data_start, edata, bss_start, ebss;

  // データ領域とBSS領域を初期化する。
  memcpy(&data_start, &erodata, (long)&edata - (long)&data_start);
  memset(&bss_start, 0, (long)&ebss - (long)&bss_start);

  // ソフトウェア・割込みベクタを初期化する
  softvec_init();
  
  // シリアルの初期化。
  serial_init(SERIAL_DEFAULT_DEVICE);

  return 0;
}

// メモリの16進ダンプ出力
static int dump(char *buf, long size) {
  long i;

  if (size < 0) {
    puts("no data.\n");
    return -1;
  }
  for (i = 0; i < size; i++) {
    putxval(buf[i], 2);
    if ((i & 0xf) == 15) {
      puts("\n");
    } else {
      if ((i & 0xf) == 7) puts(" ");
      puts(" ");
    }
  }
  puts("\n");
  
  return 0;
}

static void wait() {
  volatile long i;
  for (i = 0; i < 300000; i++)
    ;
}

int global_data = 0x10;
int global_bss;
static int static_data = 0x20;
static int static_bss;

static void printval(void) {
  puts("global_data = "); putxval(global_data, 0); puts("\n");
  puts("gobal_bss = "); putxval(global_bss, 0); puts("\n");
  puts("static_data = "); putxval(static_data, 0); puts("\n");
  puts("static_bss = "); putxval(static_bss, 0); puts("\n");
}

int main(void) {
  static char buf[16];
  static long size = -1;
  static unsigned char *loadbuf = NULL;
  static char *entry_point;
  void (*f)(void);
  extern int buffer_start; // バッファ領域を指すシンボル。リンカスクリプトで定義されている。

  INTR_DISABLE;
  
  init();
  puts("kzload (kozos boot loader) started.\n");

  while (1) {
    puts("kzload> ");
    gets(buf); // シリアルからのコマンド受信。

    if (!strcmp(buf, "load")) { // XMODEMでのダウンロード
      loadbuf = (char *)(&buffer_start);
      size = xmodem_recv(loadbuf);
      wait();
      if (size < 0) {
        puts("\nXMODEM receive error!\n");
      } else {
        puts("\nXMODEM receive succeeded.\n");
      }
    } else if (!strcmp(buf, "dump")) { // メモリの16進ダンプ
      puts("size: ");
      putxval(size, 0);
      puts("\n");
      dump(loadbuf, size);
    } else if (!strcmp(buf, "run")) { // ELF形式ファイルの実行
      entry_point = elf_load(loadbuf); // メモリ上に展開
      if (!entry_point) {
        puts("run error!\n");
      } else {
        puts("starting from entry point: ");
        putxval((unsigned long)entry_point, 0);
        puts("\n");
        f = (void (*)(void))entry_point;
        f(); // ロードしたプログラムに処理を渡す
        // ここには基本的に到達しない
      }
    } else {
      puts("unknown.\n");
    }
  }
  
  printval();
  puts("overwrite variables.\n");
  global_data = 0x20;
  global_bss = 0x30;
  static_data = 0x40;
  static_bss = 0x50;
  printval();
  
  while(1)
    ;
  
  return 0;
}
