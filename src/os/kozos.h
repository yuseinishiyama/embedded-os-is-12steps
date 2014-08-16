#ifndef _KOZOS_H_INCLUDED_
#define _KOZOS_H_INCLUDED_

#include "defines.h"
#include "syscall.h"

////////////////////////////////////////
// システムコール
////////////////////////////////////////

// スレッドの起動
kz_thread_id_t kz_run(kz_func_t func, char *name, int priority, int stacksize, int argc, char *argv[]);

void kz_exit(void);
int kz_wait(void);
int kz_sleep(void);
int kz_wakeup(kz_thread_id_t id);
kz_thread_id_t kz_getid(void);
int kz_chpri(int priority);
void *kz_malloc(int size);
int kz_kmfree(void *p);
int kz_send(kz_msgbox_id_t id, int size, char *p);
kz_thread_id_t kz_recv(kz_msgbox_id_t id, int *sizep, char **p);

////////////////////////////////////////
// ライブラリ関数
////////////////////////////////////////

// 初期スレッドを起動し、OSの動作を開始
void kz_start(kz_func_t func, char *name, int priority, int stacksize, int argc, char *argv[]);
// 致命的エラーの発生時に呼び出す
void kz_sysdown(void);
// システムコールの実行
void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param);

// ユーザースレッド
/* int test09_1_main(int argc, char *argv[]); */
/* int test09_2_main(int argc, char *argv[]); */
/* int test09_3_main(int argc, char *argv[]); */
/* extern kz_thread_id_t test09_1_id; */
/* extern kz_thread_id_t test09_2_id; */
/* extern kz_thread_id_t test09_3_id; */
/* int test10_1_main(int argc, char *argv[]); */
int test11_1_main(int argc, char *argv[]);
int test11_2_main(int argc, char *argv[]);

#endif
