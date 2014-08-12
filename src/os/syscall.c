#include "defines.h"
#include "kozos.h"
#include "syscall.h"

kz_thread_id_t kz_run(kz_func_t func, char *name, int stacksize, int argc, char *argv[]) {
  kz_syscall_param_t param;
  param.un.run.func = func;
  param.un.run.name = name;
  param.un.run.stacksize = stacksize;
  param.un.run.argc = argc;
  param.un.run.argv = argv;
  kz_syscall(KZ_SYSCALL_TYPE_RUN, &param);
  return param.un.run.ret; // システムコールの応答が構造体に格納されて返る。
}

void kz_exit(void) {
  kz_syscall(KZ_SYSCALL_TYPE_EIXT, NULL);
}
