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

static cairo_t *cr=0;
static int button_height=0;

extern uint8_t gen;

static struct e editcode[1024];
static struct e *editcode_e=editcode;

static struct voc commands = {.end=commands.heads};
static struct voc builtins = {.end=builtins.heads};
struct voc words = {.end=words.heads};

static struct editor { struct tag *tag; struct e **pos; int x, y; } editor;

static struct e final={.n=0,.t=builtin};

static struct tag *selected=0;

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

static void resize(struct tag *c) {
	cairo_text_extents_t te;
	cairo_text_extents(cr,c->s,&te);
	c->w=te.x_advance+(c->nospace?0:10); c->h=button_height+5;
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


static int nospace=0;
static struct tag *add(int x, int y, char *s, void *f, int len, int t) {
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

static void openeditor(struct tag *t) {
	editor.tag=t; editor.x=editor.tag->x; editor.y=editor.tag->y+button_height+5; editor.pos=&(editor.tag->def); draw();
}

static void do_create() { openeditor(add(100,100,"",0,0,compiled)); }

static void do_execute() {
	if(editor.tag && editor.tag->data) { execute(editor.tag->data); }
	else if(selected && selected->data) { execute(selected->data); }
	draw();
}

static void do_hexed();

static inline void change_type(enum nmflag p) {
	if(editor.tag) (*editor.pos)->t=p;
	else if(selected) selected->t=p;
	draw();
}
static void do_macro() { change_type(macro); }
static void do_normal() { change_type(compiled); }
static void do_data() { change_type(data); }


static void do_ping(void) { puts("PONG"); }

void init(cairo_t *cr1) {
	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;

	nospace=1;

	add(285,310,"h", compile_h,0,builtin);
	add(300,310,"d", compile_n,0,builtin);
	add(300,290,"0", compile_0,0,builtin);
	add(300,330,"-", compile_neg,0,builtin);

	add(320,290,"1", compile_1,0,builtin);
	add(335,290,"2", compile_2,0,builtin);
	add(350,290,"3", compile_3,0,builtin);

	add(320,310,"4", compile_4,0,builtin);
	add(335,310,"5", compile_5,0,builtin);
	add(350,310,"6", compile_6,0,builtin);

	add(320,330,"7", compile_7,0,builtin);
	add(335,330,"8", compile_8,0,builtin);
	add(350,330,"9", compile_9,0,builtin);

	add(365,290,"a", compile_a,0,builtin);
	add(365,310,"b", compile_b,0,builtin);
	add(365,330,"c", compile_c,0,builtin);

	add(380,290,"d", compile_d,0,builtin);
	add(380,310,"e", compile_e,0,builtin);
	add(380,330,"f", compile_f,0,builtin);
	nospace=0;

	add(60,310,"@", compile_fetch,0,builtin);
	add(90,310,"!", compile_store,0,builtin);

	add(220,310,"!?", compile_notif,0,builtin);
	add(30,290,"ping", do_ping,0,command);
	add(30,310,"?", do_if,0,builtin);
	add(30,330,"}", do_end,0,builtin);
	add(60,330,"dup", compile_dup,0,builtin);
	add(90,330,"drop", compile_drop,0,builtin);
	add(130,330,"1-", compile_dec,0,builtin);
	add(130,310,"1+", compile_inc,0,builtin);
	add(160,330,"{", compile_begin,0,builtin);
	add(190,330,"<<", compile_rewind,0,builtin);

	add(30,250,"load", do_load,0,command);
	add(30,270,"save", do_save,0,command);

	add(30,90,"execute", do_execute,0,command);
	add(30,70,"compile", do_compile,0,command);
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);
	final.o=add(30,130,";",do_ret,0,builtin);
	add(60,290,"nip", compile_nip,0,builtin);
	add(90,290,"+", compile_add,0,builtin);
	add(120,290,"over", compile_over,0,builtin);
	add(150,290,"swap", compile_swap,0,builtin);

	add(80,30,"go",0,0,compiled);
	add(240,320,"?+", compile_ifns,0,builtin);
	add(90,270,"-", compile_sub,0,builtin);

	add(30,150,"macro",do_macro,0,command);
	add(30,170,"normal",do_normal,0,command);
	add(30,190,"data",do_data,0,command);
	add(30,210,"hexed",do_hexed,0,command);
	add(120,270,"allot", compile_allot,0,builtin);
}

static void normalcolor()  {cairo_set_source_rgb(cr,0.5,0.9,0.5);}
static void builtincolor() {cairo_set_source_rgb(cr,0.9,0.5,0.9);}
static void macrocolor()   {cairo_set_source_rgb(cr,0.5,0.5,0.9);}
static void datacolor()    {cairo_set_source_rgb(cr,0.9,0.9,0.5);}
static void commandcolor() {cairo_set_source_rgb(cr,0.9,0.5,0.5);}

static inline void dullcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.8); }
static inline void selectcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.0); }

static inline void textcolor() { cairo_set_source_rgb(cr,0,0,0); }

static int x,y;

static inline void pad(struct tag *o) {
	cairo_rectangle(cr,x,y,o->w,o->h);
	cairo_fill(cr);
}

static inline void text(struct tag *o) {
	cairo_move_to(cr, x+(o->nospace?0:5), y+button_height);
	cairo_show_text(cr, o->s);
	cairo_stroke(cr);
}

static void drawstack() {
	char s[10];
	cairo_move_to(cr, 5, 5+button_height);
	uint32_t *p=stack;

	printf("%08x %08x\n", (uint32_t)stack, (uint32_t)stackh);

	textcolor();
	while(p<stackh+32) {
		sprintf(s,"%x ",*p);
		cairo_show_text(cr, s);
		p++;
	}
	cairo_stroke(cr);
}

static void typecolor(enum nmflag t) {
	switch(t) {
		case builtin: builtincolor(); break;
		case command: commandcolor(); break;
		case compiled: normalcolor(); break;
		case macro: macrocolor(); break;
		case data: datacolor(); break;
	}
}

static inline void shift(int *y) { if(*y>100-(button_height+5)) *y+=2*(button_height+5); }
static inline void unshift(int *y) { if(*y>100+2*(button_height+5)) *y-=2*(button_height+5); }

static void drawtag(struct tag *t) {
	x=t->x; y=t->y;
	shift(&y);
	if(t==selected) selectcolor();
	else typecolor(t->t);
	pad(t);
	textcolor(); text(t);
}


static void draweditor(struct editor *ed) {
	x=ed->x; y=100;
	dullcolor(); pad(ed->tag);
	textcolor(); text(ed->tag);
	x+=ed->tag->w;

	struct e *e=ed->tag->def;
	if(!e) return;
	for(;e;e=e->n) {
		if(e==*(ed->pos)) y-=button_height/4;
		typecolor(e->t);
		pad(e->o);
		textcolor(); text(e->o);
		if(e==*(ed->pos)) y+=button_height/4;
		x+=e->o->w;
	}
}

static struct hexeditor { struct tag *tag; int x,y,pos; } hexed;

static void do_hexed() {
	if(editor.tag) { hexed.tag=editor.tag; hexed.y=editor.y+button_height+5; hexed.x=editor.x; draw(); }
}

static inline void drawbyte(uint8_t c) {
	char *hex="0123456789abcdef";
	char b[3]={hex[c>>4],hex[c&0xf],0};
	cairo_move_to(cr,x,y+button_height);
	cairo_show_text(cr,b);
	cairo_stroke(cr);
}

static void drawhexeditor(struct hexeditor *ed) {
	x=ed->x; y=ed->y;
	typecolor(data); pad(ed->tag);
	textcolor(); text(ed->tag);
	x+=ed->tag->w;

	uint8_t *p=ed->tag->data;
	uint8_t *e=ed->tag->data;
	e+=ed->tag->len;

	int pos=0;
	while(p<e) {
		drawbyte(*p++);
		if(pos==15) {
			y+=button_height+5;
			x-=15*16;
			pos=0;
			continue;
		}
		x+=16; pos++;
	}
}

void draw() {
	if(!editor.tag) cairo_set_source_rgb(cr,1,1,1);
	else cairo_set_source_rgb(cr,0.9,0.9,0.9);
	cairo_paint(cr);

	if(editor.tag) cairo_set_source_rgb(cr,1,1,1);
	else cairo_set_source_rgb(cr,0.9,0.9,0.9);
	cairo_rectangle(cr,0,100,1000,button_height+5);
	cairo_fill(cr);

	struct tag *e;
	for(e=builtins.heads;e<builtins.end;e++) { builtincolor(); drawtag(e); }
	for(e=words.heads;e<words.end;e++) { normalcolor(); drawtag(e); }
	for(e=commands.heads;e<commands.end;e++) { commandcolor(); drawtag(e); }

	if(editor.tag) draweditor(&editor);
	if(hexed.tag) drawhexeditor(&hexed);

	drawstack();
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
	shift(&y1);
	unshift(&y1);
	cairo_set_source_rgb(cr,0.8,0.8,0.8);
	cairo_arc(cr,x1,y1,10,0,7);
	cairo_stroke(cr);
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

