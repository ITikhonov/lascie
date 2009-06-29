#include <string.h>
#include <stdio.h>
#include <cairo.h>

#include "common.h"
#include "compiler.h"
#include "lasca.h"

static cairo_t *cr=0;
int button_height=0;

struct editor { struct tag *tag; struct e **pos; int x, y; } editor;
struct tag *selected=0;
void draw();

void resize(struct tag *c) {
	cairo_text_extents_t te;
	cairo_text_extents(cr,c->s,&te);
	c->w=te.x_advance+(c->nospace?0:10); c->h=button_height+5;
}


void openeditor(struct tag *t) {
	editor.tag=t; editor.x=editor.tag->x; editor.y=editor.tag->y+button_height+5; editor.pos=&(editor.tag->def); draw();
}

void drawinit(cairo_t *cr1) {
	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;
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

void shift(int *y) { if(*y>100-(button_height+5)) *y+=2*(button_height+5); }
void unshift(int *y) { if(*y>100+2*(button_height+5)) *y-=2*(button_height+5); }

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

void do_hexed() {
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

