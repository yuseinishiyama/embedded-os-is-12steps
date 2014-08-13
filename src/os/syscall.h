#ifndef _KOZOS_SYSCALL_H_INCLUDED_
#define _KOZOS_SYSCALL_H_INCLUDED_

#include "defines.h"

typedef enum {
  KZ_SYSCALL_TYPE_RUN = 0,
  KZ_SYSCALL_TYPE_EIXT,
  KZ_SYSCALL_TYPE_WAIT,
  KZ_SYSCALL_TYPE_SLEEP,
  KZ_SYSCALL_TYPE_WAKEUP,
  KZ_SYSCALL_TYPE_GETID,
  KZ_SYSCALL_TYPE_CHPRI
} kz_syscall_type_t;

typedef struct {
  union {
    struct { // kz_run()のためのパラメータ
      kz_func_t func;
      char *name;
      int priority;
      int stacksize;
      int argc;
      char **argv;
      kz_thread_id_t ret;
    } run;
    struct { // kz_exit()のためのパラメータ
      int dummy;
    } exit;
    struct {
      int ret;
    } wait;
    struct {
      int ret;
    } sleep;
    struct {
      kz_thread_id_t id;
      int ret;
    } wakeup;
    struct {
      kz_thread_id_t ret;
    } getid;
    struct {
      int priority;
      int ret;
    } chpri;
  } un;
} kz_syscall_param_t;

#endif
