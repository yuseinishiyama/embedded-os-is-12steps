#include "defines.h"
#include "interrupt.h"
#include "kozos.h"
#include "lib.h"

kz_thread_id_t test09_1_id;
kz_thread_id_t test09_2_id;
kz_thread_id_t test09_3_id;

static int start_threads(int argc, char *argv[]) {
  test09_1_id = kz_run(test09_1_main, "test09_1", 1, 0x100, 0, NULL);
  test09_2_id = kz_run(test09_2_main, "test09_2", 2, 0x100, 0, NULL);
  test09_3_id = kz_run(test09_3_main, "test09_3", 3, 0x100, 0, NULL);

  kz_chpri(15);
  INTR_ENABLE;
  while (1) {
    asm volatile ("sleep");
  }
  return 0;
}

int main(void) {
  INTR_DISABLE; // 割込みを無効にしてから初期化を行う
  
  puts("kozos boot succeed!\n");

  // OSの動作開始
  kz_start(start_threads, "idle", 0, 0x100, 0, NULL); // 初期スレッドの起動
  //ここには到達しない。
  
  return 0;
}
