#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "serial.h"
#include "lib.h"
#include "consdrv.h"

#define CONS_BUFFER_SIZE 24

static struct consreg {
  kz_thread_id_t id; // コンソールを利用するスレッド
  int index; // 利用するシリアルの番号
  
  char *send_buf; // 送信バッファ
  char *recv_buf; // 受信バッファ
  int send_len; // 送信バッファ中のデータサイズ
  int recv_len; // 受信バッファ中のデータサイズ

  long dummy[3]; // サイズ調整
} consreg[CONSDRV_DEVICE_NUM];

// 以下二つの関数は割込み処理とスレッドから呼ばれるが
// 送信バッファを使用しており再入不可のため、スレッドから呼び出す場合は
// 排他のため割り込み禁止状態で呼ぶこと

static void send_char(struct consreg *cons) {
  int i;
  serial_send_byte(cons->index, cons->send_buf[0]);
  cons->send_len--;

  // 先頭文字を送信したので、1文字分ずらす
  for (i = 0; i < cons->send_len; i++) {
    cons->send_buf[i] = cons->send_buf[i + 1];
  }
}

static void send_string(struct consreg *cons, char *str, int len) {
  int i;
  for (i = 0; i < len; i++) { // 文字列を送信バッファにコピー
    if (str[i] == '\n')
      cons->send_buf[cons->send_len++] = '\r';
    cons->send_buf[cons->send_len++] = str[i];
  }
  if (cons->send_len && !serial_intr_is_send_enable(cons->index)) {
    serial_intr_send_enable(cons->index); // 送信割込み有効化
    send_char(cons);
  }
}

static int consdrv_intrporc(struct consreg *cons) {
  unsigned char c;
  char *p;

  if (serial_is_recv_enable(cons->index)) {
    c = serial_recv_byte(cons->index);
    if (c == '\r') // 改行コード変換
      c = '\n';

    send_string(cons, &c, 1); // エコーバック処理

    if (cons->id) {
      if (c != '\n') {
        // 改行でないなら、受信バッファにバッファリングする
        cons->recv_buf[cons->recv_len++] = c;
      } else {
        // Enterが押されたら、バッファの内容をコマンド処理スレッドに通知する。
        p = kx_kmalloc(CONS_BUFFER_SIZE);
        memcpy(p, cons->recv_buf, cons->recv_len);
        kx_send(MSGBOX_ID_CONSINPUT, cons->recv_len, p);
        cons->recv_len = 0;
      }
    }
  }

  if (serial_is_send_enable(cons->index)) {
    if (!cons->id || !cons->send_len) {
      serial_intr_send_disable(cons->index);
    } else {
      send_char(cons);
    }
  }

  return 0;
}

static void consdrv_intr(void) {
  int i;
  struct consreg *cons;

  for (i = 0; i < CONSDRV_DEVICE_NUM; i++) {
    cons = &consreg[i];
    if (cons->id) {
      if (serial_is_send_enable(cons->index) ||
          serial_is_recv_enable(cons->index))
        // 割込み処理があるならば、割込み処理を呼び出す
        consdrv_intrporc(cons);
    }
  }
}

// 初期化処理
static int consdrv_init(void) {
  memset(consreg, 0, sizeof(consreg));
  return 0;
}

// スレッドからの要求を処理する
static int consdrv_command(struct consreg *cons, kz_thread_id_t id,
                           int index, int size, char *command) {

  switch (command[0]) {
  case CONSDRV_CMD_USE:
    cons->id = id;
    cons->index = command[1] - '0';
    cons->send_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->recv_buf = kz_kmalloc(CONS_BUFFER_SIZE);
    cons->send_len = 0;
    cons->recv_len = 0;
    serial_init(cons->index);
    serial_intr_recv_enable(cons->index);
    break;

  case CONSDRV_CMD_WRITE:
    INTR_DISABLE; // 割込み禁止
    send_string(cons, command + 1, size - 1);
    INTR_ENABLE;
    break;

  default:
    break;
  }
  return 0;
}

int consdrv_main(int argc, char *argv[]) {
  int size, index;
  kz_thread_id_t id;
  char *p;

  consdrv_init();
  kz_setintr(SOFTVEC_TYPE_SERINTR, consdrv_intr);

  while (1) {
    id = kz_recv(MSGBOX_ID_CONSOUTPUT, &size, &p);
    index = p[0] - '0';
    consdrv_command(&consreg[index], id, index, size - 1, p + 1);
    kz_kmfree(p);
  }
}
