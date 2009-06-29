#include <stdint.h>

extern uint8_t gen;

extern uint32_t stackh[], *stack;

void execute(void (*f)(void));
void do_ret();
void do_compile();

void add_builtins();

