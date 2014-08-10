#include "defines.h"
#include "intr.h"
#include "interrupt.h"

// ソフトウェア・割込みベクタの初期化
int softvec_init(void) {
  softvec_type_t type;
  for(type = 0; type < SOFTVEC_TYPE_NUM; type++)
    softvec_setintr(type, NULL);
  return 0;
}

// ソフトウェア・割込みベクタの設定
int softvec_setintr(softvec_type_t type, softvec_handler_t handler) {
  SOFTVECS[type] = handler;
  return 0;
}

// 共通割込みハンドラ
void interrupt(softvec_type_t type, unsigned long sp) {
  softvec_handler_t handler = SOFTVECS[type];
  if (handler)
    handler(type, sp);
}
