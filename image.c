#include <stdio.h>
#include <string.h>

#include "common.h"
#include "draw.h"
#include "image.h"

struct word *wuser;


static FILE *f=0;

static void u8(uint8_t x) { fwrite(&x,1,1,f); }
static void u16(uint16_t x) { fwrite(&x,2,1,f); }
static void u32(uint32_t x) { fwrite(&x,4,1,f); }

static void r8(uint8_t *x) { fread(x,1,1,f); }
static void r16(uint16_t *x) { fread(x,2,1,f); }
static void r32(uint32_t *x) { fread(x,4,1,f); }

void save() {
	struct word *w;
	struct tag1 *t;
	f=fopen("save","w");

	u16(wuser - words.w);
	u16(words.end - wuser);
	for(w=wuser;w<words.end;w++) {
		u8(w->t); fwrite(w->s,8,1,f);
		u8(w->def.t); u8(w->def.nospace);
		u16(w->def.n-editcode);
	}

	u16(editcode_e-editcode);
	struct e *e=editcode;
	for(;e<editcode_e;e++) { u8(e->t); u8(e->nospace); u16(e->w - words.w); u16(e->n?e->n-editcode:0xffff); }

	u16(tags.end - tags.tags);
	for(t=tags.tags;t<tags.end;t++) { u8(t->open); u32(t->x); u32(t->y); u16(t->e->w - words.w); }

	fclose(f); f=0;

}

void load() {
	uint8_t x;
	uint16_t n;

	f=fopen("save","r");
	uint16_t builtins, userwords;
	r16(&builtins); r16(&userwords);

	struct word *w=wuser;
	words.end=wuser+userwords;
	for(;w<words.end;w++) {
		r8(&x); w->t=x; fread(&w->s,8,1,f);
		r8(&x); w->def.t=x; r8(&w->def.nospace);
		r16(&n); w->def.n=editcode+n;
		w->def.w=w;
		resize(w);
	}

	uint16_t ne;
	r16(&ne);
	editcode_e=editcode+ne;
	struct e *e=editcode;
	for(;e<editcode_e;e++) {
		r8(&x); e->t=x; r8(&e->nospace);
		r16(&n);
		if(n<builtins) { e->w=&words.w[n]; }
		else { e->w=&wuser[n-builtins]; }
		r16(&n); e->n=n==0xffff?0:(editcode+n);
	}
	
	uint16_t ntags;
	r16(&ntags);

	tags.end=tags.tags+ntags;
	struct tag1 *t=tags.tags;
	for(;t<tags.end;t++) {
		uint16_t n;
		r8(&t->open); r32(&t->x); r32(&t->y); r16(&n);
		if(n<builtins) { t->e=&words.w[n].def; }
		else { t->e=&wuser[n-builtins].def; }
	}



	draw();
}

