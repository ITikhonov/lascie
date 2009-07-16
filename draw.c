#include <string.h>
#include <stdio.h>
#include <cairo.h>

#include "draw.h"

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

int width(struct e *e) { return e->w->w+(e->nospace?0:10); }


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

static void pad(struct e *e) {
	cairo_rectangle(cr,x,y,e->w->w+(e->nospace?0:10),e->w->h);
	cairo_fill(cr);
}

static void text(struct e *e) {
	cairo_move_to(cr, x+(e->nospace?0:5), y+button_height);
	cairo_show_text(cr, e->w->s);
	cairo_stroke(cr);
}

static void drawstack() {
	char s[20];
	cairo_move_to(cr, 5, 5+button_height);
	uint32_t *p=stack;

	textcolor();

	sprintf(s,"w: %d/%d ",words.end-words.w,sizeof(words.w)/sizeof(*words.w));
	cairo_show_text(cr, s);
	sprintf(s,"t: %d/%d ",tags.end-tags.tags,sizeof(tags.tags)/sizeof(*tags.tags));
	cairo_show_text(cr, s);
	sprintf(s,"e: %d/%d | ",editcode_e-editcode,sizeof(editcode)/sizeof(*editcode));
	cairo_show_text(cr, s);

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

static void drawhex(struct tag1 *t) {
	uint8_t *p = t->e->w->data;
	if(!p) return;
	p+=t->scroll*8;

	int l = t->e->w->len;
	uint8_t *end=p+l;
	int r,i;
	char s[4];
	x=t->x; y=t->y+button_height+5;
	typecolor(data);
	cairo_rectangle(cr,x,y,16*8+5,4*(button_height)+5);
	cairo_fill(cr);
	textcolor();
	for(r=0;r<4;r++) {
		x=t->x+5; y+=button_height;
		for(i=0;i<8;i++) {
			if(p>=end) return;
			cairo_move_to(cr,x,y);
			sprintf(s,"%02x",*p++);
			cairo_show_text(cr, s);
			x+=16;
		}
	}
	cairo_stroke(cr);
	
}

static void drawlist(struct e *e, int isopen) {
	for(;e;e=e->n) {
		typecolor(e->t); pad(e);
		textcolor(); text(e);
		if(selected==e) { commandcolor(); cairo_rectangle(cr,x,y,width(e),e->w->h); cairo_stroke(cr); }
		if(!isopen) return;
		x+=width(e);
	}
}

static void drawtag(struct tag1 *t) {
	x=t->x; y=t->y;
	struct e *e=t->e;
	drawlist(e,t->open);
	if(t->open && t->e->t == data) drawhex(t);
}

void draw() {
        cairo_set_source_rgb(cr,1,1,1);
        cairo_paint(cr);

	struct tag1 *t;
	for(t=tags.tags;t<tags.end;t++) { drawtag(t); }

	drawstack();
}

