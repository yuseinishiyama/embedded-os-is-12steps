#ifndef _INTERRUPT_H_INCLUDED_
#define _INTERRUPT_H_INCLUDED_

extern char softvec;
#define SOFTVEC_ADDR (&softvec)

typedef short softvec_type_t;

typedef void (*softvec_handler_t) (softvec_type_t type, unsigned long sp);

// ソフトウェア・割込みベクタの位置
#define SOFTVECS ((softvec_handler_t *)SOFTVEC_ADDR)


#define INTR_ENABLE asm volatile ("andc.b #0x3c, ccr") // 割り込み有効化
#define INTR_DISABLE asm volatile ("orc.d #0xc0, ccr") // 割り込み無効化

int softvec_init(void);

// ソフトウェア・割込みベクタの設定用関数
int softvec_setintr(softvec_type_t type, softvec_handler_t handler);

// 共通割込みハンドラ
void interrupt(softvec_type_t type, unsigned long sp);

#endif
