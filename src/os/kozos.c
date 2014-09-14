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

/* メッセージ・バッファ */
typedef struct _kz_msgbuf {
  struct _kz_msgbuf *next;
  kz_thread *sender;            /* メッセージを送信したスレッド */
  struct {                      /* メッセージのパラメータ保存領域 */
    int size;
    char *p;
  } param;
} kz_msgbuf;

/* メッセージ・ボックス */
typedef struct _kz_msgbox {
  kz_thread *receiver;
  kz_msgbuf *head;
  kz_msgbuf *tail;

  /* 構造体のサイズが2の累乗になるように調整 */
  long dummy[1];
} kz_msgbox;

static struct {
  kz_thread *head;
  kz_thread *tail;
} readyque[PRIORITY_NUM];

static kz_thread *current; // 現在実行中のスレッド
static kz_thread threads[THREAD_NUM];
static kz_handler_t handlers[SOFTVEC_TYPE_NUM];
static kz_msgbox msgboxes[MSGBOX_ID_NUM];

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

static void sendmsg(kz_msgbox *mboxp, kz_thread *thp, int size, char *p) {
  kz_msgbuf *mp;
  mp = (kz_msgbuf *)kzmem_alloc(sizeof(*mp));
  if (mp == NULL)
    kz_sysdown();
  /* パラメータ設定 */
  mp->next       = NULL;
  mp->sender     = thp;
  mp->param.size = size;
  mp->param.p    = p;

  /* メッセージボックスのキューの末尾にメッセージを追加 */
  if (mboxp->tail) {
    mboxp->tail->next = mp;
  } else {
    mboxp->head = mp;
  }
  mboxp->tail = mp;
}

static void recvmsg(kz_msgbox *mboxp) {
  kz_msgbuf *mp;
  kz_syscall_param_t *p;

  /* メッセージ・ボックスの先頭にあるメッセージを抜き出す */
  mp = mboxp->head;
  mboxp->head = mp->next;
  if (mboxp->head == NULL)
    mboxp->tail = NULL;
  mp->next = NULL;

  /* メッセージを受信するスレッドに返す値を設定する */
  /* kz_recv()の戻り値としてスレッドに返す値 */
  p = mboxp->receiver->syscall.param;
  p->un.recv.ret = (kz_thread_id_t)mp->sender;
  if (p->un.recv.sizep)
    *(p->un.recv.sizep) = mp->param.size;
  if (p->un.recv.pp)
    *(p->un.recv.pp) = mp->param.p;

  /* 受信待ちスレッドの登録を解除 */
  mboxp->receiver = NULL;

  /* メッセージバッファの解放 */
  kzmem_free(mp);
}

static int thread_send(kz_msgbox_id_t id, int size, char*p) {
  kz_msgbox *mboxp = &msgboxes[id];

  putcurrent();
  sendmsg(mboxp, current, size, p);

  /* 送信したメッセージボックスで受信待ちしているスレッドがある場合には受信処理を行う */
  if (mboxp->receiver) {
    current = mboxp->receiver;
    recvmsg(mboxp);
    putcurrent();
  }

  return size;
}

static kz_thread_id_t thread_recv(kz_msgbox_id_t id, int *sizep, char **pp) {
  kz_msgbox *mboxp = &msgboxes[id];

  /* 他のスレッドが既に受信待ち */
  if (mboxp->receiver)
    kz_sysdown();

  /* 受信待ちスレッドに設定 */
  mboxp->receiver = current;

  /* メッセージボックスにメッセージがない場合は、スレッドをスリープさせて受信待ちに入る */
  if (mboxp->head == NULL)
    return -1;

  recvmsg(mboxp);               /* メッセージの受信処理 */
  putcurrent();                 /* メッセージを受信できたので、レディー状態にする */

  return current->syscall.param->un.recv.ret;
}

////////////////////////////////////////
// システムコールの処理(kx_setintr():割込みハンドラ登録)
////////////////////////////////////////

static int thread_setintr(softvec_type_t type, kz_handler_t handler) {
  static void thread_intr(softvec_type_t type, unsigned long sp);

  // 割り込み時にOSのハンドラが呼ばれるようにソフトウェア・割込みベクタを設定する
  softvec_setintr(type, thread_intr);

  // OS側から呼び出す割込みハンドラを登録
  handlers[type] = handler;
  putcurrent(); // 処理後にレディー・キューに接続し直す
  
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
  case KZ_SYSCALL_TYPE_SEND:
    p->un.send.ret = thread_send(p->un.send.id,
                                 p->un.send.size,
                                 p->un.send.p);
    break;
  case KZ_SYSCALL_TYPE_RECV:
    p->un.recv.ret = thread_recv(p->un.recv.id,
                                 p->un.recv.sizep,
                                 p->un.recv.pp);
    break;
  case KZ_SYSCALL_TYPE_SETINTR:  /* kz_setintr */
    p->un.setintr.ret = thread_setintr(p->un.setintr.type, p->un.setintr.handler);
    break;
  default:
    break;
  }
}

static void syscall_proc(kz_syscall_type_t type, kz_syscall_param_t *p) {
  getcurrent();
  call_functions(type, p);
}

/* サービスコールの処理 */
static void srvcall_proc(kz_syscall_type_t type, kz_syscall_param_t *p) {
  current = NULL;
  call_functions(type, p);
}
  
////////////////////////////////////////
// 割込み処理
////////////////////////////////////////

/* スレッドのスケジューリング */
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

  dispatch(&current->context); // 本体はstartup.sにあり、アセンブラで記述されている。
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
  memset(msgboxes, 0, sizeof(msgboxes));

  // 割込みハンドラの登録
  thread_setintr(SOFTVEC_TYPE_SYSCALL, syscall_intr);
  thread_setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr);

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

// システムコール呼び出し用ライブラリ関数
void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param) {
  current->syscall.type = type;
  current->syscall.param = param;
  asm volatile ("trapa #0"); // トラップ命令により割込みを発生させる
}

// サービス・コール呼び出し用ライブラリ関数
void kz_srvcall(kz_syscall_type_t type, kz_syscall_param_t *param) {
  srvcall_proc(type, param);
}

