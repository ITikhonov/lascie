#include <string.h>
#include <stdio.h>
#include <cairo.h>

#include "common.h"
#include "compiler.h"
#include "lasca.h"

static cairo_t *cr=0;

void draw();

void resize(struct word *w) {
	cairo_text_extents_t te;
	cairo_text_extents(cr,w->s,&te);
	w->w=te.x_advance; w->h=button_height+5;
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
static void macrocolor()   {cairo_set_source_rgb(cr,0.5,0.5,0.9);}
static void datacolor()    {cairo_set_source_rgb(cr,0.9,0.9,0.5);}
static void commandcolor() {cairo_set_source_rgb(cr,0.9,0.5,0.5);}

static inline void dullcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.8); }

static inline void textcolor() { cairo_set_source_rgb(cr,0,0,0); }

static int x,y;

static inline void pad(struct tag1 *t) {
	cairo_rectangle(cr,x,y,t->w->w+(t->nospace?0:10),t->w->h);
	cairo_fill(cr);
}

static inline void text(struct tag1 *t) {
	cairo_move_to(cr, x+(t->nospace?0:5), y+button_height);
	cairo_show_text(cr, t->w->s);
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

static void typecolor(enum tagtype t) {
	switch(t) {
		case command: commandcolor(); break;
		case normal: normalcolor(); break;
		case macro: macrocolor(); break;
		case data: datacolor(); break;
	}
}

void shift(int *y) { if(*y>100-(button_height+5)) *y+=2*(button_height+5); }
void unshift(int *y) { if(*y>100+2*(button_height+5)) *y-=2*(button_height+5); }

static void drawtag(struct tag1 *t) {
	x=t->x; y=t->y;
	typecolor(t->t); pad(t);
	textcolor(); text(t);
}

void draw() {
        cairo_set_source_rgb(cr,1,1,1);
        cairo_paint(cr);

	struct tag1 *t;
	for(t=tags.tags;t<tags.end;t++) { drawtag(t); }

	drawstack();
}

