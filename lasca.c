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

uint32_t stack_place[16];
uint32_t *stack = stack_place-1;

enum nmtype { compiled, data, macro, command };

struct c1 { uint8_t op; uint32_t x,y,w,h; char s[8]; uint8_t t; void *data; uint32_t l; int open; };
struct c2 { uint8_t op; int32_t p; };
union c { uint8_t *op; struct c1 *c1; struct c2 *c2; };

struct c1 words[256], *words_e = words;
struct c1 *user=0;
struct e { struct e *n; struct c1 *o; } editcode[1024];
struct e *editcode_e=editcode;
struct e heads[256], *heads_e = heads;
struct e *userh=0;

struct e *final=0;

void draw();

void do_exit() { exit(0); }

void savelist(struct e *e, FILE *f) {
	if(e->o<user) return;
	fwrite(&e->o->x,4,1,f);
	fwrite(&e->o->y,4,1,f);
	fwrite(&e->o->s,8,1,f);
	fwrite(&e->o->t,1,1,f);

	for(e=e->n;e;e=e->n) {
		int16_t n=e->o-user;
		fwrite(&n,2,1,f);
	}
}

void do_save() {
	struct e *e;
	FILE *f=fopen("save","w");
	for(e=heads;e<heads_e;e++) { savelist(e,f); }
	fclose(f);
}


void resize(struct c1 *c) {
	cairo_text_extents_t te;
	cairo_text_extents(cr,c->s,&te);
	c->w=te.x_advance+10; c->h=button_height+5;
}


int loadone(FILE *f) {
	if(!fread(&words_e->x,4,1,f)) return 0;
	fread(&words_e->y,4,1,f);
	fread(&words_e->s,8,1,f);
	fread(&words_e->t,1,1,f);
	resize(words_e);
	heads_e->o=words_e++;

	struct e *e=heads_e++;

	for(;;) {
		int16_t n;
		fread(&n,2,1,f);
		editcode_e->o=user+n;
		e->n=editcode_e;
		e=editcode_e++;
		
		if(n==-1) break;
	}
	e->n=0;

	return 1;
}

void do_load() {
	words_e=user;
	heads_e=userh;
	editcode_e=editcode;

	FILE *f=fopen("save","r");
	while(loadone(f)) ;
	fclose(f);

	draw();
}



struct e *reg(struct c1 *c, int x, int y, char *s, void *f, int len, int t) {
	c->op=1; c->x=x; c->y=y; c->t=t; c->data=f; c->l=len;
	strncpy(c->s,s,7);
	resize(c);

	struct e *h=heads_e++;
	h->o=c;
	h->n=t==compiled?final:0;
	return h;
}

struct e *add(int x, int y, char *s, void *f, int len, int t) {
	return reg(words_e++,x,y,s,f,len,t);
}

struct e *editp=0;
struct e *edit=0;

void do_create() { editp=0; edit=add(100,100,"",0,0,compiled); draw(); }

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
	resize(words);

	int n;
	char s[2]={0,0};
	for(n='0';n<='9';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }
	for(n='a';n<='f';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }

	add(30,250,"load", do_load,0,command);
	add(30,270,"save", do_save,0,command);
	add(30,290,"ping", do_ping,0,command);
	add(30,70,"compile", do_compile,0,command);
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);
	final=add(0,0,";",do_ping,0,command);
	user=words_e;
	userh=heads_e;
}

int dull=0;

void padcolor() { if(!dull) { cairo_set_source_rgb(cr,0.5,0.9,0.5); } else { cairo_set_source_rgb(cr,0.8,0.8,0.8); } }
void textcolor() { cairo_set_source_rgb(cr,0.0,0.0,0.0); }

int x,y;

void label(struct c1 *o) {
	padcolor();
	cairo_rectangle(cr,x,y,o->w,o->h);
	cairo_fill(cr);

	textcolor();
	cairo_move_to(cr, x+5, y+button_height);
	cairo_show_text(cr, o->s);
	cairo_stroke(cr);

}

void drawlist(struct e *e) {
	x=e->o->x; y=e->o->y;
	for(e=e;e;e=e->n) {
		dull = edit && edit!=e;
		label(e->o);
		x+=e->o->w;
	}
}

void draw() {
	cairo_set_source_rgb(cr,255,255,255);
	cairo_paint(cr);

	struct e *e;
	for(e=heads;e<heads_e;e++) { drawlist(e); }

}

union ic { uint8_t *b; int32_t *i; void *v; } cc;
uint8_t ccode[65535];
struct { int32_t *p; struct c1 *w; } decs[1024], *dec=decs;

void do_compile() {
}


int clicklist(struct e *e, int x1,int y1) {
	x=e->o->x+e->o->w; y=e->o->y;
	if(x1<=x) {
		if(editp) {
			struct e *e1=editcode_e++;
			e1->o=e->o;
			e1->n=edit;
			editp->n=e1;
			editp=e1;
			draw(); return 1;
		} else {
			switch(e->o->t) {
			case command: { void (*f)(void)=(void *)e->o->data; f(); return 1; }
			case compiled: editp=0; edit=e; draw(); return 1;
			}
		}
	}

	struct e *p=e;
	for(e=e->n;e;e=e->n) {
		x+=e->o->w;
		if(x1<=x) { editp=p; edit=e; draw(); return 1; }
		p=e;
	}
	return 0;
}

void button(int x1,int y1) {
	struct e *e;
	for(e=heads;e<heads_e;e++) {
		if(e->o->y<=y1 && y1<=e->o->y+e->o->h && x1>e->o->x) { if(clicklist(e,x1,y1)) return; }
	}
	editp=0; edit=0;
	draw();
}

void key(int k) {
	if(edit) {
		if(editp==0) {
			struct e *e=edit;
			switch(k) {
				case XK_BackSpace:
					e->o->s[strlen(e->o->s)-1]=0;
					resize(e->o);
					draw();
					break;
				case XK_Return:
					editp=edit; edit=edit->n; draw(); break;
				case '0'...'9':
				case 'a'...'z': {
					int l=strlen(e->o->s);
					e->o->s[l]=k;
					e->o->s[l+1]=0;
					resize(e->o);
					draw();
					}
			}
		} else {
			switch(k) {
				case XK_Delete:
					if(edit->n) editp->n=edit->n; edit=edit->n; draw(); break;
			}
		}
	}
}
