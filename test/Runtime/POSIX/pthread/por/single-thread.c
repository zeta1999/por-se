// RUN: %clang %s -emit-llvm %O0opt -g -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --posix-runtime --exit-on-error --log-por-events %t.bc 2>&1 | FileCheck %s

int main(void) {
  return 0;


  // CHECK: POR event: thread_init with current thread [[M_TID:tid<[0-9,]+>]] and initialized thread [[M_TID]]
  // CHECK-DAG: POR event: lock_acquire with current thread [[M_TID]] on mutex [[FS_LID:[0-9]+]]
  // CHECK-DAG: POR event: thread_exit with current thread [[M_TID]] and exited thread [[M_TID]]
}