#include <stdio.h>
#include <string.h>

#include "common.h"
#include "draw.h"
#include "image.h"

struct tag1 *tuser;
struct word *wuser;
struct e *euser;


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

	u16(words.end - words.w);
	for(w=words.w;w<words.end;w++) { u8(w->t); fwrite(w->s,8,1,f); }

	for(w=words.w;w<words.end;w++) {
		struct e *e=&w->def;
		for(;e;e=e->n) { u8(e->t); u8(e->nospace); u16(e->w - words.w); } u8(0xff);
	}

	u16(tags.end - tags.tags);
	for(t=tags.tags;t<tags.end;t++) { u8(t->open); u32(t->x); u32(t->y); u16(t->e->w - words.w); }

	fclose(f); f=0;

}

static struct word *resolve() {
	char s[8];
	fread(s,8,1,f);
	struct word *w;
	for(w=words.w;w<words.end;w++) { if(!memcmp(s,w->s,8)) return w; }
	return 0;
}

void load() {
        tags.end=tags.tags;;
        words.end=wuser;
        editcode_e=editcode;

	f=fopen("save","r");
	uint8_t x;
	uint16_t x2;
	struct word *mh[256], **m, **me;

	r16(&x2); me=mh+x2;

	printf("words: %d\n", x2);

	for(m=mh;m<me;m++) {
		r8(&x);
		if(x==builtin) { *m=resolve(); continue; }
		*m=newword();
		fread((*m)->s,8,1,f);
		(*m)->t=x;
		resize(*m);
	}

	for(m=mh;m<me;m++) {
		printf("loading: %u %s\n", m-mh, (*m)->s);
		r8(&x); (*m)->def.t=x; r8(&(*m)->def.nospace); r16(&x2); (*m)->def.w=*m;
		for(;;) {
			r8(&x); if(x==0xff) break;
			struct e *e=editcode_e++;
			e->t=x;
			r8(&e->nospace);
			r16(&x2);
			e->w=mh[x2];
		}
	}

	r16(&x2);
	struct tag1 *t=tags.tags, *te=tags.tags+x2-1;
	for(;t<te;t=tags.end++) {
		printf("loading tag %u\n", t-tags.tags);
		r8(&(t->open)); r32(&t->x); r32(&t->y);
		r16(&x2); t->e = &mh[x2]->def;
		printf("loaded tag %u %s\n", x2, mh[x2]->s);
	}

	draw();
}

