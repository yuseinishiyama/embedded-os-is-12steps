#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "syscall.h"
#include "memory.h"
#include "lib.h"

#define THREAD_NUM 6
#define PRIORITY_NUM 16
#define THREAD_NAME_SIZE 15

typedef struct _kz_context {
  uint32 sp;
} kz_context;

typedef struct _kz_thread {
  struct _kz_thread *next;
  char name[THREAD_NAME_SIZE + 1];
  int priority;
  char *stack;
  uint32 flags;
  #define KZ_THREAD_FLAG_READY (1 << 0)
  struct { // スレッドのスタートアップに渡すパラメータ
    kz_func_t func; // スレッドのメイン関数
    int argc;
    char **argv;
  } init;

  struct { // システムコールの発行時に利用するパラメータ
    kz_syscall_type_t type;
    kz_syscall_param_t *param;
  } syscall;

  kz_context context;
} kz_thread;

static struct {
  kz_thread *head;
  kz_thread *tail;
} readyque[PRIORITY_NUM];

static kz_thread *current; // 現在実行中のスレッド
static kz_thread threads[THREAD_NUM];
static kz_handler_t handlers[SOFTVEC_TYPE_NUM];

// スレッドのディスパッチ(実体はstartup.sに)
void dispatch(kz_context *context);

////////////////////////////////////////
// レディ・キューの操作
////////////////////////////////////////

// カレント・スレッドをレディー・キューから抜き出す
static int getcurrent(void) {
  if (current == NULL) {
    return -1;
  }

  // READY状態でないならなばなにもしない。
  if (!(current->flags & KZ_THREAD_FLAG_READY)) {
    return 1;
  }

  readyque[current->priority].head = current->next;
  if (readyque[current->priority].head == NULL) {
    readyque[current->priority].tail = NULL;
  }
  current->flags &= ~KZ_THREAD_FLAG_READY; // READYビットを落とす。
  current->next = NULL;

  return 0;                         
}

// カレント・スレッドをレディ・キューに繋げる
static int putcurrent(void) {
  if (current == NULL) {
    return -1;
  }
  // READY状態ならばなにもしない。
  if (current->flags & KZ_THREAD_FLAG_READY) {
    return 1;
  }
  if (readyque[current->priority].tail) {
    readyque[current->priority].tail->next = current;
  } else {
    readyque[current->priority].head = current;
  }
  readyque[current->priority].tail = current;
  current->flags |= KZ_THREAD_FLAG_READY; // READYビットを立てる。

  return 0;
}

////////////////////////////////////////
// スレッドの起動と終了
////////////////////////////////////////

// スレッドの終了
static void thread_end(void) {
  kz_exit();
}

// スレッドのスタート・アップ
static void thread_init(kz_thread *thp) {
  // スレッドのメイン関数を呼び出す
  thp->init.func(thp->init.argc, thp->init.argv);
  // メイン関数から戻ってきたら、スレッドを終了
  thread_end();
}

static kz_thread_id_t thread_run(kz_func_t func,
                                 char *name,
                                 int priority,
                                 int stacksize,
                                 int argc,
                                 char *argv[]) {
  int i;
  kz_thread *thp;
  uint32 *sp;
  extern char userstack; // リンカ・スクリプトで定義されるスタック領域
  static char *thread_stack = &userstack; // ユーザー・スタックに利用される領域

  // 空いているTCB(タスク・コントロール・ブロック)を検索
  for (i = 0; i < THREAD_NUM; i++) {
    thp = &threads[i];
    if (!thp->init.func)
      break;
  }
  if (i == THREAD_NUM)
    return -1;

  memset(thp, 0, sizeof(*thp)); // TCBをゼロクリア

  // TCBの設定
  strcpy(thp->name, name);
  thp->next      = NULL;
  thp->priority  = priority;
  thp->flags     = 0;
  thp->init.func = func;
  thp->init.argc = argc;
  thp->init.argv = argv;

  // スタック領域を獲得
  memset(thread_stack, 0, stacksize);
  thread_stack += stacksize;
  
  thp->stack = thread_stack;

  // スタックの初期化
  sp = (uint32 *)thp->stack;

  // thread_init()の戻り先としてthread_end()を設定
  *(--sp) = (uint32)thread_end;

  // プログラムカウンタを設定
  // スレッドの優先度がゼロの場合には、割込み禁止スレッドとする。
  *(--sp) = (uint32)thread_init | ((uint32)(priority ? 0 : 0xc0) << 24);

  *(--sp) = 0; // ER6
  *(--sp) = 0; // ER5
  *(--sp) = 0; // ER4
  *(--sp) = 0; // ER3
  *(--sp) = 0; // ER2
  *(--sp) = 0; // ER1

  // thread_init()に渡す引数
  *(--sp) = (uint32)thp; // ER0

  thp->context.sp = (uint32)sp;

  // システム・コールを呼び出したスレッドをレディ・キューに戻す
  putcurrent();

  // 新規作成したスレッドを、レディ・キューに接続する
  current = thp;
  putcurrent();

  return (kz_thread_id_t)current;
}

// スレッドの終了。
static int thread_exit(void) {
  puts(current->name);
  puts(" EXIT.\n");
  memset(current, 0, sizeof(*current));
  return 0;
}

// スレッドの実行権放棄。
static int thread_wait(void) {
  putcurrent(); // レディ・キューから一旦外して末尾に接続し直す。
  return 0;
}

// スレッドのスリープ。
static int thread_sleep(void) {
  return 0; // レディ・キューから外されたままになるので、スケジューリングされなくなる。
}

static int thread_wakeup(kz_thread_id_t id) {
  putcurrent();

  current = (kz_thread *)id;
  putcurrent();

  return 0;
}

// スレッドID取得。
static kz_thread_id_t thread_getid(void) {
  putcurrent();
  return (kz_thread_id_t)current;
}

// スレッドの優先度変更。
static int thread_chpri(int priority) {
  int old = current->priority;
  if (priority >= 0)
    current->priority = priority;
  putcurrent();
  return old;
}

static void *thread_kzmalloc(int size) {
  putcurrent();
  return kzmem_alloc(size);
}

static int thread_kmfree(char *p) {
  kzmem_free(p);
  putcurrent();
  return 0;
}

////////////////////////////////////////
// 割り込みハンドラの登録
////////////////////////////////////////

static int setintr(softvec_type_t type, kz_handler_t handler) {
  static void thread_intr(softvec_type_t type, unsigned long sp);

  // 割り込み時にOSのハンドラが呼ばれるようにソフトウェア・割込みベクタを設定する
  softvec_setintr(type, thread_intr);

  // OS側から呼び出す割込みハンドラを登録
  handlers[type] = handler;

  return 0;
}

////////////////////////////////////////
// システム・コールの呼び出し
////////////////////////////////////////

static void call_functions(kz_syscall_type_t type, kz_syscall_param_t *p) {
  switch (type) {
  case KZ_SYSCALL_TYPE_RUN: // kz_run()
    p->un.run.ret = thread_run(p->un.run.func,
                               p->un.run.name,
                               p->un.run.priority,
                               p->un.run.stacksize,
                               p->un.run.argc,
                               p->un.run.argv);
    break;
  case KZ_SYSCALL_TYPE_EIXT: // kz_exit()
    thread_exit();
    break;
  case KZ_SYSCALL_TYPE_WAIT:
    p->un.wait.ret = thread_wait();
    break;
  case KZ_SYSCALL_TYPE_SLEEP:
    p->un.sleep.ret = thread_sleep();
    break;
  case KZ_SYSCALL_TYPE_WAKEUP:
    p->un.wakeup.ret = thread_wakeup(p->un.wakeup.id);
    break;
  case KZ_SYSCALL_TYPE_GETID:
    p->un.getid.ret = thread_getid();
    break;
  case KZ_SYSCALL_TYPE_CHPRI:
    p->un.chpri.ret = thread_chpri(p->un.chpri.priority);
    break;
  case KZ_SYSCALL_TYPE_KMALLOC:
    p->un.kmalloc.ret = thread_kzmalloc(p->un.kmalloc.size);
    break;
  case KZ_SYSCALL_TYPE_KMFREE:
    p->un.kmfree.ret = thread_kmfree(p->un.kmfree.p);
    break;
  default:
    break;
  }
}

static void syscall_proc(kz_syscall_type_t type, kz_syscall_param_t *p) {
  getcurrent();
  call_functions(type, p);
}

////////////////////////////////////////
// 割込み処理
////////////////////////////////////////

static void schedule(void) {
  int i;

  // 優先度の高い順にレディ・キューを見ていく。
  for (i = 0; i < PRIORITY_NUM; i++) {
    if (readyque[i].head) // 見つかった。
      break;
  }
  
  if (i == PRIORITY_NUM) // 見つからなかった。
    kz_sysdown();
  
  current = readyque[i].head;
}

// システムコールの呼び出し
static void syscall_intr(void) {
  syscall_proc(current->syscall.type, current->syscall.param);
}

// ソフトウェアエラーの発生
static void softerr_intr(void) {
  puts(current->name);
  puts(" DOWN.\n");
  getcurrent(); // レディーキューから外す
  thread_exit(); // スレッドを終了する
}

// 割込みハンドラの入り口
static void thread_intr(softvec_type_t type, unsigned long sp) {
  // カレント・スレッドのコンテキストを保存
  current->context.sp = sp;

  if (handlers[type])
    handlers[type]();

  schedule();

  dispatch(&current->context);
  // ここには到達しない
}

////////////////////////////////////////
// 初期スレッドの起動
////////////////////////////////////////

void kz_start(kz_func_t func,
              char *name,
              int priority,
              int stacksize,
              int argc,
              char *argv[]) {
  kzmem_init();                 /* 動的メモリの初期化 */
  current = NULL;

  memset(readyque, 0, sizeof(readyque));
  memset(threads, 0, sizeof(threads));
  memset(handlers, 0, sizeof(handlers));

  // 割込みハンドラの登録
  setintr(SOFTVEC_TYPE_SYSCALL, syscall_intr);
  setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr);

  current = (kz_thread *)thread_run(func, name, priority, stacksize, argc, argv);

  dispatch(&current->context);
  // ここには到達しない。
}

// 致命的なエラーが発生した場合
void kz_sysdown(void) {
  puts("system error!\n");
  while (1) // 無限ループに入り停止する。
    ;
}

void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param) {
  current->syscall.type = type;
  current->syscall.param = param;
  asm volatile ("trapa #0"); // トラップ命令により割込みを発生させる
}


