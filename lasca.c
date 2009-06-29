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
#include "compiler.h"
#include "draw.h"

extern uint8_t gen;

static struct e editcode[1024];
struct e *editcode_e=editcode;

struct voc commands = {.end=commands.heads};
struct voc builtins = {.end=builtins.heads};
struct voc words = {.end=words.heads};

static struct e final={.n=0,.t=builtin};

void draw();

static void do_exit() { exit(0); }

static inline void saveword(struct tag *t, FILE *f) {
	fwrite(&t->t,1,1,f);
	fwrite(&t->x,4,1,f);
	fwrite(&t->y,4,1,f);
	fwrite(t->s,8,1,f);

	struct e *e=t->def;
	for(;e;e=e->n) {
		fwrite(&e->o->t,1,1,f);
		struct voc *v;
		switch(e->o->t) {
			case builtin: v=&builtins; break;
			default: v=&words; break;
		}
		uint16_t n = e->o - v->heads;
		fwrite(&n,2,1,f);
	}
	fwrite("\xff",1,1,f);
}

static void do_save() {
	struct tag *t;
	FILE *f=fopen("save","w");
	for(t=words.heads;t<words.end;t++) { saveword(t,f); }
	fwrite("\xff",1,1,f);
	fclose(f);
}

static void do_load() {
	FILE *f=fopen("save","r");

	words.end=words.heads;

	for(;;) {
		uint8_t tp;
		fread(&tp,1,1,f);
		if(tp==0xff) break;

		struct tag *t=words.end++;

		t->t=tp;
		fread(&t->x,4,1,f);
		fread(&t->y,4,1,f);
		fread(t->s,8,1,f);
		printf("name %s\n",t->s);
		resize(t);
		t->gen=gen;

		struct e **p=&t->def;

		for(;;) {
			uint8_t tp;
			fread(&tp,1,1,f);
			printf("  type %x\n",tp);
			if(tp==0xff) break;

			struct voc *v;
			switch(tp) {
				case builtin: v=&builtins; break;
				default: v=&words; break;
			}

			uint16_t n;
			fread(&n,2,1,f);
			printf("  n    %d\n",n);

			struct e *e=editcode_e++;
			e->o=v->heads+n;
			e->t=tp;
			e->n=0;
			*p=e;
			p=&(e->n);
		}
	}
	fclose(f);
	draw();
}


int nospace=0;
struct tag *add(int x, int y, char *s, void *f, int len, int t) {
	struct tag *c;
	switch(t) {
	case builtin: c=builtins.end++; break;
	case command: c=commands.end++; break;
	default: c=words.end++;
	}

	c->x=x; c->y=y; c->t=t; c->data=f; c->len=len; c->nospace=nospace; c->gen=gen;
	strncpy(c->s,s,7);
	resize(c);
	c->def=t==compiled?&final:0;
	return c;
}

static void do_create() { openeditor(add(100,100,"",0,0,compiled)); }

static void do_execute() {
	if(editor.tag && editor.tag->data) { execute(editor.tag->data); }
	else if(selected && selected->data) { execute(selected->data); }
	draw();
}

static inline void change_type(enum nmflag p) {
	if(editor.tag) (*editor.pos)->t=p;
	else if(selected) selected->t=p;
	draw();
}
static void do_macro() { change_type(macro); }
static void do_normal() { change_type(compiled); }
static void do_data() { change_type(data); }


static void do_ping(void) { puts("PONG"); }

void init(cairo_t *cr) {
	drawinit(cr);
	
	add(30,290,"ping", do_ping,0,command);

	add(30,250,"load", do_load,0,command);
	add(30,270,"save", do_save,0,command);

	add(30,90,"execute", do_execute,0,command);
	add(30,70,"compile", do_compile,0,command);
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);
	final.o=add(30,130,";",do_ret,0,builtin);

	add(80,30,"go",0,0,compiled);

	add(30,150,"macro",do_macro,0,command);
	add(30,170,"normal",do_normal,0,command);
	add(30,190,"data",do_data,0,command);
	add(30,210,"hexed",do_hexed,0,command);

	add_builtins();
}

void go() {
	if(words.heads->data) { execute(words.heads->data); }
}

