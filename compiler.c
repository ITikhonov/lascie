#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <alsa/asoundlib.h>
#include <pthread.h>

#include <math.h>

#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#define NDEBUG
#include <assert.h>

#include "common.h"

extern uint8_t gen;

extern struct voc words;

static union ic { uint8_t *b; int8_t *c; int32_t *i; void *v; } cc;
static uint8_t ccode[65535];
static struct { int32_t *p; struct tag *w; } decs[1024], *dec=decs;

uint32_t stackh[32]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}, *stack=stackh+30;

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
void compile_dup() {
	*cc.b++=0x8d; *cc.b++=0x76; *cc.b++=0xfc;
	*cc.b++=0x89; *cc.b++=0x06;
}

static int n=0, n_sign=1, base=10;
void compile_neg() { n_sign=-1; }
void compile_0() { n=n*base; }
void compile_1() { n=n*base+1; }
void compile_2() { n=n*base+2; }
void compile_3() { n=n*base+3; }
void compile_4() { n=n*base+4; }
void compile_5() { n=n*base+5; }
void compile_6() { n=n*base+6; }
void compile_7() { n=n*base+7; }
void compile_8() { n=n*base+8; }
void compile_9() { n=n*base+9; }
void compile_n() { compile_dup(); compile_imm(n_sign*n); n=0; n_sign=1; base=10; }

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

void compile_a() { convert_h(); n=n*16+10; }
void compile_b() { convert_h(); n=n*16+11; }
void compile_c() { convert_h(); n=n*16+12; }
void compile_d() { convert_h(); n=n*16+13; }
void compile_e() { convert_h(); n=n*16+14; }
void compile_f() { convert_h(); n=n*16+15; }
void compile_h() { convert_h(); compile_n(); }

void do_ret() { *cc.b++=0xc3; }
static int8_t *fwjumps[8], **fwjump;
void do_if() {
	*cc.b++=0x75;
	*fwjump++=cc.c++;
}
void compile_notif() { *cc.b++=0x74; *fwjump++=cc.c++; }
void compile_ifns() { *cc.b++=0x79; *fwjump++=cc.c++; }

void do_end() {
	--fwjump;
	**fwjump = cc.b - (uint8_t*)((*fwjump)+1);
}

static int8_t *bwjumps[8], **bwjump;
void compile_begin() {
	*bwjump++=cc.c;
}
void compile_rewind() {
	--bwjump;
	*cc.b++=0xeb;
	*cc.c++=*bwjump-(cc.c+1);
}


void compile_drop() {
	*cc.b++=0xad;
}

void compile_fetch() {
	*cc.b++=0x8b;
	*cc.b++=0x00;
}

void compile_store() {
	*cc.b++=0x89;
	*cc.b++=0xc2;
	compile_drop();
	*cc.b++=0x89;
	*cc.b++=0x02;
	compile_drop();
}

void compile_dec() { *cc.b++=0x48; }
void compile_inc() { *cc.b++=0x40; }
void compile_nip() { *cc.b++=0x8d; *cc.b++=0x76; *cc.b++=0x04; }
void compile_add() { *cc.b++=0x03; *cc.b++=0x06; compile_nip(); }
void compile_sub() { *cc.b++=0x2b; *cc.b++=0x06; compile_nip(); }
void compile_over() { compile_dup(); *cc.b++=0x8b; *cc.b++=0x46; *cc.b++=0x04; }
void compile_swap() { *cc.b++=0x87; *cc.b++=0x06; }

void compile_call(void *a) { *cc.b++=0xe8; *cc.i++=((uint8_t*)a)-(cc.b+4); }

static void delay(struct tag *w) {
	*cc.i=0;
	dec->p=cc.i++; dec->w=w; dec++;
}

static struct tag *current;

static void do_allot() {
	register uint32_t *stack asm("esi");
	void *data=(void*)stack[1];
	uint32_t len=stack[0];
	printf("realloc %08x", (uint32_t)data);
	stack[1]=(uint32_t)realloc(data,len);
	printf(" -> %08x (%d)\n", (uint32_t)stack[1], len);
	current->len=len;
}

void compile_allot() {
	compile_dup();
	compile_call(do_allot);
	compile_drop();
	compile_drop();
}

static uint8_t *beg;

static inline void compilelist(struct tag *t) {
	printf("compile %s\n", t->s);
	fwjump=fwjumps;
	bwjump=bwjumps;
	current = t;

	beg=cc.b;

	t->gen++;

	struct e *e=t->def;
	for(;e;e=e->n) {
		switch(e->t) {
		case builtin: { void (*f)(void)=e->o->data; f(); } break;
		case data:
			compile_dup();
			assert(e->o->gen==gen);
			compile_imm((uint32_t)e->o->data);
			break;
		case macro:
			assert(e->o->gen==gen);
			execute(e->o->data);
			break;
		default:
			*cc.b++=0xe8;
			if(e->o->gen!=gen) { delay(e->o); }
			else if(e->o==t) { *cc.i++=((uint8_t*)e->o->data)-(beg+4); }
			else { *cc.i++=((uint8_t*)e->o->data)-(cc.b+4); }
		}
	}
	if(t->t==data) {
		*(--stack)=(uint32_t)(t->data);
		execute((void *)beg);
		t->data=(void *)(*(stack++));
	}
	else t->data=beg;
}

static struct tag *plan[256];

static void do_plan() {
	struct tag *t;
	struct e *depth[10],**d;
	struct tag **p=plan;
	gen+=2;
	for(t=words.heads;t<words.end;t++) {
		if(t->gen==gen) continue;
		d=depth;
		struct e *e=t->def;
		t->gen++;
		printf("root: %s\n", t->s);
		for(;;) {
			for(;e;e=e->n) {
				if(e->o->t==builtin) continue;
				if(e->o->gen==gen) continue; // compiled
				if(e->o->gen==gen-1) {
					if(e->t==macro) { printf("circular 1 %s\n", e->o->s); return; }
					struct e **p=d-1;
					for(;p>=depth && (*p)->o!=e->o;p--) {
						if((*p)->t == macro) { printf("circular 2 %s\n", e->o->s); return; }
					}
					continue; // will backpatch it
				}
				break;
			}
			if(!e) {
				if(--d<depth) break;
				(*d)->o->gen++;
				printf(" < %d/%d %*s'%s'\n", (*d)->o->gen, gen, (d-depth)*3, "", (*d)->o->s);
				*p++=(*d)->o;
				e=(*d)->n;
			} else {
				printf(" > %d/%d %*s'%s'\n", e->o->gen, gen, (d-depth)*3, "", e->o->s);
				e->o->gen++;
				*d++=e;
				e=e->o->def;
			}
		}
		t->gen++;
		*p++=t;
	}
	*p=0;
}

void do_compile() {
	cc.b=ccode;
	struct tag **t;
	do_plan();
	dec=decs;
	gen++;
	for(t=plan;*t;t++) { compilelist(*t); }
	while(--dec>=decs) {
		*dec->p=((uint8_t *)dec->w->data-(uint8_t*)(dec->p+1));
	}
}

