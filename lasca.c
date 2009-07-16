#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lasca.h"

#include "common.h"
#include "compiler.h"
#include "draw.h"
#include "image.h"

extern uint8_t gen;

static struct e final={.n=0,.t=macro};

static void do_exit() { exit(0); }

int nospace=0;
struct e *add(int x, int y, char *s, void *f, int len, enum tagtype tt, enum wordtype wt) {
	struct word *w=newword();
	struct tag1 *t=tags.end++;
	
	t->x=x;
	t->y=y;
	t->open=0;

	t->e=&w->def;

	t->e->t=tt;
	t->e->nospace=nospace;
	t->e->w=w;

	w->gen=gen;
	w->data=f;
	w->len=len;
	w->t=wt;
	strncpy(w->s,s,7);
	resize(w);

	if(wt==compiled) {
		struct e *e=editcode_e++;
		*e=final;
		w->def.n=e;
	} else {
		w->def.n=0;
	}

	return t->e;
}

static void do_create() { selected=add(100,100,"",0,0,normal,compiled); draw(); }

static void do_compile1() { do_compile(); draw(); }

static void do_execute() {
	if(selected && selected->w->data) execute(selected->w->data);
	draw();
}

static inline void change_type(enum tagtype p) {
	if(selected) selected->t=p;
	draw();
}
static void do_macro() { change_type(macro); }
static void do_normal() { change_type(compiled); }
static void do_data() { change_type(data); }


void init(cairo_t *cr) {
	editcode_e=editcode;
	drawinit(cr);
	
	add(30,90,"execute", do_execute,0,command,builtin);
	add(30,70,"compile", do_compile1,0,command,builtin);
	add(30,50,"exit", do_exit,0,command,builtin);
	add(30,30,"create", do_create,0,command,builtin);
	final.w=add(30,130,";",do_ret,0,macro,builtin)->w;


	add(30,150,"macro",do_macro,0,command,builtin);
	add(30,170,"normal",do_normal,0,command,builtin);
	add(30,190,"data",do_data,0,command,builtin);

	add(30,110,"load", load,0,command,builtin);
	add(30,130,"save", save,0,command,builtin);

	add_builtins();

	wuser=words.end;

	add(80,30,"go",0,0,normal,compiled);
}

void go() {
}

