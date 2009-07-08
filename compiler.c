#include <stdio.h>
#include <stdlib.h>
#define NDEBUG
#include <assert.h>

#include "compiler.h"

#include "common.h"
#include "lasca.h"

static union ic { uint8_t *b; int8_t *c; int32_t *i; void *v; } cc;
static uint8_t ccode[65535];
static struct { int32_t *p; struct word *w; } decs[1024], *dec=decs;

void execute(void (*f)(void)) {
	asm volatile (
		"movl stack,%%esi; lodsl;"
		"call *%%edx;"
		"leal -4(%%esi),%%esi;"
		"movl %%eax,(%%esi);"
		"movl %%esi,stack"
		: : "d" (f) : "esi","eax","memory"
	);
}


static void compile_imm(int32_t x) { *cc.b++=0xb8; *cc.i++=x; }
static void compile_dup() {
	*cc.b++=0x8d; *cc.b++=0x76; *cc.b++=0xfc;
	*cc.b++=0x89; *cc.b++=0x06;
}

static int n=0, n_sign=1, base=10;
static void compile_neg() { n_sign=-1; }
static void compile_0() { n=n*base; }
static void compile_1() { n=n*base+1; }
static void compile_2() { n=n*base+2; }
static void compile_3() { n=n*base+3; }
static void compile_4() { n=n*base+4; }
static void compile_5() { n=n*base+5; }
static void compile_6() { n=n*base+6; }
static void compile_7() { n=n*base+7; }
static void compile_8() { n=n*base+8; }
static void compile_9() { n=n*base+9; }
static void compile_n() { compile_dup(); compile_imm(n_sign*n); n=0; n_sign=1; base=10; }

static void convert_h() {
	if(base==16) return;
	int x=n%10; n/=10;
	x+=(n%10)*0x10; n/=10;
	x+=(n%10)*0x100; n/=10;
	x+=(n%10)*0x1000; n/=10;
	x+=(n%10)*0x10000; n/=10;
	x+=(n%10)*0x100000; n/=10;
	x+=(n%10)*0x1000000; n/=10;
	x+=(n%10)*0x10000000; n/=10;
	n=x;
	base=16;
}

static void compile_a() { convert_h(); n=n*16+10; }
static void compile_b() { convert_h(); n=n*16+11; }
static void compile_c() { convert_h(); n=n*16+12; }
static void compile_d() { convert_h(); n=n*16+13; }
static void compile_e() { convert_h(); n=n*16+14; }
static void compile_f() { convert_h(); n=n*16+15; }
static void compile_h() { convert_h(); compile_n(); }

void do_ret() { *cc.b++=0xc3; }
static int8_t *fwjumps[8], **fwjump;
static void do_if() {
	*cc.b++=0x75;
	*fwjump++=cc.c++;
}
static void compile_notif() { *cc.b++=0x74; *fwjump++=cc.c++; }
static void compile_ifns() { *cc.b++=0x79; *fwjump++=cc.c++; }

static void do_end() {
	--fwjump;
	**fwjump = cc.b - (uint8_t*)((*fwjump)+1);
}

static int8_t *bwjumps[8], **bwjump;
static void compile_begin() {
	*bwjump++=cc.c;
}
static void compile_rewind() {
	--bwjump;
	*cc.b++=0xeb;
	*cc.c++=*bwjump-(cc.c+1);
}


static void compile_drop() {
	*cc.b++=0xad;
}

static void compile_fetch() {
	*cc.b++=0x8b;
	*cc.b++=0x00;
}

static void compile_store() {
	*cc.b++=0x89;
	*cc.b++=0xc2;
	compile_drop();
	*cc.b++=0x89;
	*cc.b++=0x02;
	compile_drop();
}

static void compile_dec() { *cc.b++=0x48; }
static void compile_inc() { *cc.b++=0x40; }
static void compile_nip() { *cc.b++=0x8d; *cc.b++=0x76; *cc.b++=0x04; }
static void compile_add() { *cc.b++=0x03; *cc.b++=0x06; compile_nip(); }
static void compile_sub() { *cc.b++=0x2b; *cc.b++=0x06; compile_nip(); }
static void compile_over() { compile_dup(); *cc.b++=0x8b; *cc.b++=0x46; *cc.b++=0x04; }
static void compile_swap() { *cc.b++=0x87; *cc.b++=0x06; }

static void compile_call(void *a) { *cc.b++=0xe8; *cc.i++=((uint8_t*)a)-(cc.b+4); }

static void delay(struct word *w) {
	*cc.i=0;
	dec->p=cc.i++; dec->w=w; dec++;
}

static struct word *current;

static void do_allot() {
	register uint32_t *stack asm("esi");
	void *data=(void*)stack[1];
	uint32_t len=stack[0];
	printf("realloc %08x", (uint32_t)data);
	stack[1]=(uint32_t)realloc(data,len);
	printf(" -> %08x (%d)\n", (uint32_t)stack[1], len);
	current->len=len;
}

static void compile_allot() {
	compile_dup();
	compile_call(do_allot);
	compile_drop();
	compile_drop();
}

static uint8_t *beg;

static inline void compilelist(struct word *w) {
	printf("compile %s\n", w->s);
	fwjump=fwjumps;
	bwjump=bwjumps;
	current = w;

	beg=cc.b;

	w->gen++;

	struct e *e=w->def.n;
	for(;e;e=e->n) {
		switch(e->t) {
		case data:
			compile_dup();
			assert(e->w->gen==gen);
			compile_imm((uint32_t)e->w->data);
			break;
		case macro:
			assert(e->w->gen==gen);
			if(e->w->t==builtin) { void (*f)(void)=e->w->data; f(); }
			else { execute(e->w->data); }
			break;
		case normal:
			*cc.b++=0xe8;
			if(e->w->gen!=gen) { delay(e->w); }
			else if(e->w==w) { *cc.i++=((uint8_t*)e->w->data)-(beg+4); }
			else { *cc.i++=((uint8_t*)e->w->data)-(cc.b+4); }
			break;
		case command: break;
		}
	}
	if(w->def.t==data) {
		*(--stack)=(uint32_t)(w->data);
		execute((void *)beg);
		w->data=(void *)(*(stack++));
	}
	else w->data=beg;
}

static struct word *plan[256];

static void do_plan() {
	struct word *w;
	struct e *depth[10],**d;
	struct word **p=plan;
	gen+=2;
	for(w=words.w;w<words.end;w++) {
		if(w->t==builtin) continue;
		if(w->gen==gen) continue;
		d=depth;
		struct e *e=w->def.n;
		w->gen++;
		printf("root: %s\n", w->s);
		for(;;) {
			for(;e;e=e->n) {
				if(e->w->t==builtin) continue;
				if(e->w->gen==gen) continue; // already compiled
				if(e->w->gen==gen-1) {
					if(e->t==macro) { printf("circular 1 %s\n", e->w->s); return; }
					struct e **p=d-1;
					for(;p>=depth && (*p)->w!=e->w;p--) {
						if((*p)->t == macro) { printf("circular 2 %s\n", e->w->s); return; }
					}
					continue; // will backpatch it during compile
				}
				break;
			}
			if(!e) {
				if(--d<depth) break;
				(*d)->w->gen++;
				printf(" < %d/%d %*s'%s'\n", (*d)->w->gen, gen, (d-depth)*3, "", (*d)->w->s);
				*p++=(*d)->w;
				e=(*d)->n;
			} else {
				printf(" > %d/%d %*s'%s'\n", e->w->gen, gen, (d-depth)*3, "", e->w->s);
				e->w->gen++;
				*d++=e;
				e=e->w->def.n;
			}
		}
		w->gen++;
		*p++=w;
	}
	*p=0;
}

void do_compile() {
	cc.b=ccode;
	struct word **w;
	do_plan();
	dec=decs;
	gen++;
	for(w=plan;*w;w++) { compilelist(*w); }
	while(--dec>=decs) {
		*dec->p=((uint8_t *)dec->w->data-(uint8_t*)(dec->p+1));
	}
}

void add_builtins() {
	nospace=1;
	add(285,310,"h", compile_h,0,macro,builtin);
	add(300,310,"d", compile_n,0,macro,builtin);
	add(300,290,"0", compile_0,0,macro,builtin);
	add(300,330,"-", compile_neg,0,macro,builtin);

	add(320,290,"1", compile_1,0,macro,builtin);
	add(335,290,"2", compile_2,0,macro,builtin);
	add(350,290,"3", compile_3,0,macro,builtin);

	add(320,310,"4", compile_4,0,macro,builtin);
	add(335,310,"5", compile_5,0,macro,builtin);
	add(350,310,"6", compile_6,0,macro,builtin);

	add(320,330,"7", compile_7,0,macro,builtin);
	add(335,330,"8", compile_8,0,macro,builtin);
	add(350,330,"9", compile_9,0,macro,builtin);

	add(365,290,"a", compile_a,0,macro,builtin);
	add(365,310,"b", compile_b,0,macro,builtin);
	add(365,330,"c", compile_c,0,macro,builtin);

	add(380,290,"d", compile_d,0,macro,builtin);
	add(380,310,"e", compile_e,0,macro,builtin);
	add(380,330,"f", compile_f,0,macro,builtin);
	nospace=0;

	add(60,310,"@", compile_fetch,0,macro,builtin);
	add(90,310,"!", compile_store,0,macro,builtin);

	add(220,310,"!?", compile_notif,0,macro,builtin);
	add(30,310,"?", do_if,0,macro,builtin);
	add(30,330,"}", do_end,0,macro,builtin);
	add(60,330,"dup", compile_dup,0,macro,builtin);
	add(90,330,"drop", compile_drop,0,macro,builtin);
	add(130,330,"1-", compile_dec,0,macro,builtin);
	add(130,310,"1+", compile_inc,0,macro,builtin);
	add(160,330,"{", compile_begin,0,macro,builtin);
	add(190,330,"<<", compile_rewind,0,macro,builtin);

	add(60,290,"nip", compile_nip,0,macro,builtin);
	add(90,290,"+", compile_add,0,macro,builtin);
	add(120,290,"over", compile_over,0,macro,builtin);
	add(150,290,"swap", compile_swap,0,macro,builtin);

	add(240,320,"?+", compile_ifns,0,macro,builtin);
	add(90,270,"-", compile_sub,0,macro,builtin);

	add(120,270,"allot", compile_allot,0,macro,builtin);
}

