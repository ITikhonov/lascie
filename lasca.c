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

int max(int x,int y) { return x>y?x:y; }
int min(int x,int y) { return x>y?y:x; }

static cairo_t *cr=0;
int button_height=0;

uint8_t drawcode[65535];
uint8_t *drawcode_e=drawcode;

uint32_t stack_place[16];
uint32_t *stack = stack_place-1;

enum nmtype { compiled, data, macro, command };

struct c1 { uint8_t op; uint32_t x,y,w,h; char s[8]; uint8_t t; void *data; uint32_t l; int open; };
struct c2 { uint8_t op; int32_t p; };
union c { uint8_t *op; struct c1 *c1; struct c2 *c2; };

struct e { struct e *p,*n; union c c; } editcode[1024] = {{0,0,{0}}};
struct e *editcode_e=editcode+1;
struct e *elast=editcode;

void draw();

void do_exit() { exit(0); }

void resize(struct c1 *c) {
	cairo_text_extents_t te;
	cairo_text_extents(cr,c->s,&te);
	c->w=te.x_advance+10; c->h=button_height+5;
}

struct e *append(struct e *w) {
	struct e *e=editcode_e++;
	e->p=w;
	e->n=w->n;
	w->n=e;
	if(e->n) { e->n->p=e; } else { elast=e; }
	return e;
}

struct e *add(int x, int y, char *s, void *f, int len, int t) {
	union c c;
	c.op=drawcode_e;
	c.c1->op=1; c.c1->x=x; c.c1->y=y; c.c1->t=t; c.c1->data=f; c.c1->l=len;
	strncpy(c.c1->s,s,7);
	resize(c.c1);

	struct e *e=append(elast);
	e->c.op=c.op;
	c.c1++;
	drawcode_e=c.op;

	return e;
}

struct e *edit=0;

void do_create() { edit=add(100, 100, "", 0, 0, compiled); draw(); }

void do_compile();


void do_ping(void) { puts("PONG"); }

void alsa_init();

int8_t hexext;

void init(cairo_t *cr1) {
	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;

	int n;
	char s[2]={0,0};
	for(n='0';n<='9';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }
	for(n='a';n<='f';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }

	add(30,290,"ping", do_ping,0,command);
	add(30,70,"compile", do_compile,0,command);
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);

}

int dull=0;

void padcolor() { if(!dull) { cairo_set_source_rgb(cr,0.5,0.9,0.5); } else { cairo_set_source_rgb(cr,0.8,0.8,0.8); } }
void textcolor() { cairo_set_source_rgb(cr,0.0,0.0,0.0); }

struct e *e;
union c c;
int x,y;
struct c1 *o;

void load() {
	switch(*c.op) {
	case 1: x=c.c1->x; y=c.c1->y; o=c.c1; return;
	case 2: o=(struct c1 *)(drawcode+c.c2->p); return;
	}
}

void first() { e=editcode; c.op=0; }
int next() {
	e=e->n;
	if(!e) return 0;
	if(c.op) { x+=o->w; }
	c.op=e->c.op;
	load();
	return 1;
}

void label() {
	padcolor();
	cairo_rectangle(cr,x,y,o->w,o->h);
	cairo_fill(cr);

	textcolor();
	cairo_move_to(cr, x+5, y+button_height);
	cairo_show_text(cr, o->s);
	cairo_stroke(cr);

}

void draw() {
	cairo_set_source_rgb(cr,255,255,255);
	cairo_paint(cr);

	first();
	while(next()) {
		dull=edit&&e!=edit;
		label();
	}

}

union ic { uint8_t *b; int32_t *i; void *v; } cc;
uint8_t ccode[65535];
struct { int32_t *p; struct c1 *w; } decs[1024], *dec=decs;

void do_compile() {
	cc.b=ccode;
	first();
	int open=0;
	while(next()) {
		switch(*c.op) {
		case 1: if(open) *cc.b++=0xc3; if(o->t==compiled) { open=1; o->data=cc.b; } break;
		case 2: *cc.b++=0xe8;
			if((o->t==compiled && o->data && o->data<=cc.v)||o->t==command) {
				*cc.i++=(uint8_t *)o->data-(cc.b+4);
			} else {
				dec->p=cc.i; dec++->w=o;
				*cc.i++=0;
			}
		}
	}
	*cc.b++=0xc3;

	while(--dec>=decs) { *dec->p=(uint8_t *)dec->w->data-(uint8_t*)(dec->p+1); }

	FILE *f=fopen("dump","w");
	fwrite(ccode,65535,1,f);
	fclose(f);
}

void button(int x1,int y1) {
	first();
	while(next()) {
		if(x<=x1 && x1<=x+o->w  && y<=y1 && y1<=y+o->h) {
			switch(*c.op) {
			case 1: 
				if(edit) {
					edit=append(edit);
					edit->c.op=drawcode_e;
					edit->c.c2->op=2;
					edit->c.c2->p=c.op-drawcode;
					drawcode_e=(uint8_t*)(edit->c.c2+1);
					draw();
					return;
				}

				switch(o->t) {
					case command: { void (*f)(void)=(void *)o->data; f(); } return;
					case compiled: edit=e; draw(); return;
				}
			case 2:  edit=e; draw(); return;
			}
		}
	}

	if(edit && *edit->c.op==1) {
		edit->c.c1->x=x1;
		edit->c.c1->y=y1;
	} else {
		edit=0;
	}
	draw();
}


void key(int k) {
	if(edit) {
		if(*edit->c.op==1) {
			union c c; c.op=edit->c.op;
			switch(k) {
			case XK_BackSpace:
				c.c1->s[strlen(c.c1->s)]=0;
				resize(c.c1);
				draw();
				break;
			case XK_Return:
				c.op=0; draw(); break;
			case '0'...'9':
			case 'a'...'z': {
				int l=strlen(c.c1->s);
				c.c1->s[l]=k;
				c.c1->s[l+1]=0;
				resize(c.c1);
				draw();
				}
			}
				
		} else {
			switch(k) {
			case XK_Delete:
				edit->p->n=edit->n;
				edit->n->p=edit->p;
				edit=edit->p;
				draw();
				return;
			}
		}
	}
}
