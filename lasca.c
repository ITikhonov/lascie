#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>

#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

static cairo_t *cr=0;
int button_height=0;

struct bt { int x,y; char s[8];  void (*act)(void); int x2,y2;} *bts;
struct bt *bte;

struct ops { void (*button)(int,int); void (*key)(int); };

#define OPSBK(n) void button_##n(int,int); void key_##n(int); struct ops n = {button_##n, key_##n};
#define OPSK(n) void key_##n(int); struct ops n = {0, key_##n};
#define OPSB(n) void button_##n(int,int); struct ops n = {button_##n, 0};

OPSB(choose)
OPSB(move1)
OPSK(rename1)

struct ops *ops=&choose;

struct bt *clicked=0;
struct bt *which=0;
int pos;

void draw_button(struct bt *);
void draw();

void hit(int x,int y) {
	struct bt *p=bte; for(;p!=bts;) { --p; if(x>p->x&&x<p->x2 && y>p->y&&y<p->y2) {clicked=p; return;} }
	clicked=0;
}


void do_choose() { which=clicked; draw(); }
void do_exit() { exit(0); }
void do_move() { if(which) { ops=&move1; }; }
void do_rename() { if(which) { pos=0; which->s[0]=0; ops=&rename1; draw(); } }
void do_define() { if(which) { ops=&define; draw(); } }

void button_choose(int x,int y) { hit(x,y); if(clicked) clicked->act(); }
void button_move1(int x,int y) { which->x=x&(~0x7); which->y=y&(~0x7); ops=&choose; draw(); }

void do_create() {
	which=bte;
	which->x=bts[0].x2+5; which->y=bts[0].y; which->s[0]='\0'; which->act=do_choose; bte++;
	draw();
	do_rename();
}

void key_rename1(int k) {
	if(k==XK_Return) { ops=&choose; return; }
	if(k==XK_BackSpace) { if(pos>0) { which->s[--pos]='\0'; draw(); } return; }
	if(pos<8-1) { which->s[pos++]='a'+(k-XK_a); which->s[pos]='\0'; }
	draw_button(which);
}

void init(cairo_t *cr1) {
	static struct bt bts_hold[100] = {
		{30,30,"create", do_create},
		{30,50,"exit", do_exit},
		{30,70,"move", do_move},
		{30,90,"rename", do_rename},
		{30,110,"define", do_define},
	};

	bts=bts_hold; bte=bts+4;

	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;
}

void draw_button(struct bt *bt) {
	char *s=bt->s;

	cairo_text_extents_t te;
	cairo_text_extents(cr,s,&te);

	if(bt==which) { cairo_set_source_rgb(cr,0.9,0.5,0.5); } else { cairo_set_source_rgb(cr,0.5,0.9,0.5);
	}
	bt->x2=bt->x+te.width+10; bt->y2=bt->y+button_height+5;
	cairo_rectangle(cr,bt->x,bt->y,bt->x2-bt->x,bt->y2-bt->y);
	cairo_fill(cr);


	cairo_set_source_rgb(cr,0,0,0);
	int x=bt->x+5,y=bt->y+button_height;
	cairo_move_to (cr, x, y);
	cairo_show_text (cr, s);
	cairo_stroke(cr);
}

void draw() {
	cairo_save(cr);
	cairo_set_source_rgb(cr,255,255,255);
	cairo_paint(cr);
	struct bt *p=bts; for(;p<bte;p++) { draw_button(p); }
	cairo_restore(cr);
}

void button(int x,int y) { if(ops->button) ops->button(x,y); }
void key(int k)		 { if(ops->key) ops->key(k); }

