#include <stdint.h>

extern uint32_t stackh[], *stack;

void execute(void (*f)(void));
void compile_neg();
void compile_dup();
void compile_0();
void compile_1();
void compile_2();
void compile_3();
void compile_4();
void compile_5();
void compile_6();
void compile_7();
void compile_8();
void compile_9();
void compile_n();
void compile_a();
void compile_b();
void compile_c();
void compile_d();
void compile_e();
void compile_f();
void compile_h();
void do_ret();
void do_if();
void compile_notif();
void compile_ifns();
void do_end();
void compile_begin();
void compile_rewind();
void compile_drop();
void compile_fetch();
void compile_store();
void compile_dec();
void compile_inc();
void compile_nip();
void compile_add();
void compile_sub();
void compile_over();
void compile_swap();
void compile_call(void *a);
void compile_allot();

void do_compile();

