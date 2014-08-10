#ifndef _INTR_H_INCLUDED_
#define _INTR_H_INCLUDED_

#define SOFTVEC_TYPE_NUM 3 // ソフトウェア・割り込みベクタの種別の個数

#define SOFTVEC_TYPE_SOFTERR 0 // ソフトウェア・エラー
#define SOFTVEC_TYPE_SYSCALL 1 // システム・コール
#define SOFTVEC_TYPE_SERINTR 2 // シリアル割込み

#endif
