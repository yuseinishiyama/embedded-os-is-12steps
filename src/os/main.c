#include "defines.h"
#include "interrupt.h"
#include "kozos.h"
#include "lib.h"

static int start_threads(int argc, char *argv[]) {
  kz_run(test08_1_main, "command", 0x100, 0, NULL);
  return 0;
}

int main(void) {
  INTR_DISABLE; // 割込みを無効にしてから初期化を行う
  
  puts("kozos boot succeed!\n");

  // OSの動作開始
  kz_start(start_threads, "start", 0x100, 0, NULL); // 初期スレッドの起動
  //ここには到達しない。
  
  return 0;
}
