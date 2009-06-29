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
static struct e *editcode_e=editcode;

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

static inline int clickcommand(struct tag *e, int x1,int y1) {
	if(!(e->y<=y1 && y1<=e->y+e->h && x1>e->x && x1<e->x+e->w)) return 0;
	void (*f)(void)=(void *)e->data; f(); return 1;
}


static inline int clicktag(struct tag *t, int x1,int y1) {
	if(!(t->y<=y1 && y1<=t->y+t->h && x1>t->x && x1<t->x+t->w)) return 0;
	if(editor.tag) {
		struct e *e=editcode_e++;
		e->o=t;
		e->t=t->t;
		e->n=*editor.pos;
		*editor.pos=e;
		editor.pos=&e->n;
	} else if(t->t!=builtin && t==selected) {
		openeditor(t);
	} else {
		selected=t;
	}
	draw();
	return 1;
}

static int x, y;

static inline int clickeditor(struct editor *ed, int x1, int y1) {
	if(!(y1>=100 && y1<=100+button_height+5 && x1>ed->x)) return 0;

	x=ed->x+ed->tag->w; y=ed->y;
	if(x1<=x) { ed->tag=0; draw(); return 1; }

	struct e **p=&ed->tag->def;
	for(;*p;p=&(*p)->n) {
		x+=(*p)->o->w;
		if(x1<=x) { ed->pos=p; draw(); return 1; }
	}
	return 0;
}

void release(int x1,int y1) {
	struct tag *e;
	if(editor.tag) { if(clickeditor(&editor,x1,y1)) return; }

	unshift(&y1);
	for(e=commands.heads;e<commands.end;e++) { if(clickcommand(e,x1,y1)) return; }

	for(e=words.heads;e<words.end;e++) { if(clicktag(e,x1,y1)) return; }
	for(e=builtins.heads;e<builtins.end;e++) { if(clicktag(e,x1,y1)) return; }

	if(editor.tag) { editor.tag=0; }
	else if(selected) { selected->x=x1 & 0xfffffff0; selected->y=y1 & 0xfffffff0; draw(); return; }

	draw();
}

void button(int x1, int y1) {
}

void key(int k) {
	if(editor.tag) {
		switch(k) {
			case XK_BackSpace:
				editor.tag->s[strlen(editor.tag->s)-1]=0;
				resize(editor.tag);
				draw();
				break;
			case XK_Delete:
				if((*editor.pos)->n) { *(editor.pos)=(*editor.pos)->n; draw(); } break;
			case XK_Return:
				editor.tag=0; draw(); break;
			case '0'...'9':
			case 'a'...'z': {
				char *s=editor.tag->s;
				int l=strlen(s);
				s[l]=k;
				s[l+1]=0;
				resize(editor.tag);
				draw();
				}
		}
	}
}

void go() {
	if(words.heads->data) { execute(words.heads->data); }
}

