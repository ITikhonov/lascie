#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>

#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

static cairo_t *cr=0;
int button_height=0;

struct nm { char s[8]; int def[10]; int data; } nms[100];
struct nm *nme=nms;

struct bt { int x,y; struct nm *n; void (*act)(void); int x2,y2,open,pos;} bts[100];
struct bt *bte=bts;

struct ops { void (*button)(int,int); void (*key)(int); };

#define OPSBK(n) void button_##n(int,int); void key_##n(int); struct ops n = {button_##n, key_##n};
#define OPSK(n) void key_##n(int); struct ops n = {0, key_##n};
#define OPSB(n) void button_##n(int,int); struct ops n = {button_##n, 0};

OPSB(edit)
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
void do_rename() { if(which) { pos=0; which->n->s[0]=0; ops=&rename1; draw(); } }
void do_open() { if(which) { which->open=!which->open; draw(); } }

int editpos=-1;

void do_edit() {
	if(!which) return;
	which->open=1;
	editpos=0;
	ops=&edit;
	draw();
}

void do_down() { if(which) { if((which->pos+8)<which->n->def[1]) { which->pos+=8; draw(); } } }
void do_up() { if(which) { if((which->pos-8)>=0) { which->pos-=8; draw(); } } }

void button_edit(int x, int y) {
	hit(x,y); if(clicked) {
		if(which->n->def[editpos]==-1) { which->n->def[editpos+1]=-1; }
		which->n->def[editpos++]=clicked->n-nms;
		draw();
	} else { ops=&choose; }
}
void button_choose(int x,int y) { hit(x,y); if(clicked) clicked->act(); }
void button_move1(int x,int y) { which->x=x&(~0x7); which->y=y&(~0x7); ops=&choose; draw(); }

void do_create() {
	which=bte; which->n=nme; bte++; nme++;
	which->x=bts[0].x2+5; which->y=bts[0].y; which->n->s[0]=0; which->act=do_choose; which->n->def[0]=-1;
	draw();
	do_rename();
}

void key_rename1(int k) {
	if(k==XK_Return) { ops=&choose; return; }
	if(k==XK_BackSpace) { if(pos>0) { which->n->s[--pos]='\0'; draw(); } return; }
	if(pos<8-1) { which->n->s[pos++]='a'+(k-XK_a); which->n->s[pos]='\0'; }
	draw_button(which);
}

void add(int x, int y, char *s, void (*f)(void)) {
	struct bt *w=bte; w->n=nme; bte++; nme++;
	w->x=x; w->y=y; strncpy(w->n->s,s,8); w->act=f;
	w->n->def[0]=-1;
	w->n->data=0;
}

unsigned char hexext[256];

void init(cairo_t *cr1) {
	add(30,30,"create", do_create);
	add(30,50,"exit", do_exit);
	add(30,70,"move", do_move);
	add(30,90,"rename", do_rename);
	add(30,110,"open", do_open);
	add(30,130,"edit", do_edit);
	add(30,150,"up", do_up);
	add(30,170,"down", do_down);

	add(90,90,"test", do_choose);
	bte[-1].n->def[0]=0;
	bte[-1].n->def[1]=1;
	bte[-1].n->def[2]=2;
	bte[-1].n->def[3]=-1;

	add(150,90,"dtest", do_choose);
	char *p=(char *)((bte[-1].n->def[0])=(int)malloc(17));
	{ int i; for(i=0;i<17;i++) p[i]=i; }
	bte[-1].n->def[1]=17;
	bte[-1].n->data=1;
	bte[-1].pos=8;

	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;

	char s[4];
	int n;
	for(n=0;n<0x100;n++) { sprintf(s," %02hhx",n); cairo_text_extents(cr,s,&te); hexext[n]=te.x_advance; }
}

void draw_button(struct bt *bt) {
	char *s=bt->n->s;
	int ws[11], wn=1;
	char ns[10];

	cairo_text_extents_t te;
	cairo_text_extents(cr,s,&te);
	ws[0]=te.width;

	if(bt->open) {
		if(!bt->n->data) {
			int *n=bt->n->def;
			int *p=ws+1;
			while(*n != -1) {
				cairo_text_extents(cr,nms[*n++].s,&te);
				*p++=te.width;
				wn++;
			}
		} else {
			sprintf(ns,"%x:",bt->n->def[1]);
			cairo_text_extents(cr,ns,&te);
			ws[1]=te.x_advance;
			wn++;

			char *p=(char *)(bt->n->def[0])+bt->pos;
			int m=bt->n->def[1];
			if(m>8) m=8;
			char *e=p+m;
			ws[2]=0;
			wn++;
			while(p<e) { ws[2]+=hexext[(unsigned char)(*p++)]; }
		}
	}

	if(bt==which) { cairo_set_source_rgb(cr,0.9,0.5,0.5); } else { cairo_set_source_rgb(cr,0.5,0.9,0.5); }

	bt->x2=bt->x+5+ws[0];
	int i=0; for(i=1;i<wn;i++) { bt->x2+=5+ws[i]; }
	bt->x2+=5;

	bt->y2=bt->y+button_height+5;
	cairo_rectangle(cr,bt->x,bt->y,bt->x2-bt->x,bt->y2-bt->y);
	cairo_fill(cr);

	cairo_set_source_rgb(cr,0,0,0);
	int x=bt->x+5,y=bt->y+button_height;
	cairo_move_to (cr, x, y);
	cairo_show_text (cr, s);
	cairo_stroke(cr);

	if(bt->open) {
		x+=ws[0];
		if(!bt->n->data) {
			int *n=bt->n->def;
			int *p=ws+1;
			while(*n != -1) {
				x+=5;
				if(ops==&edit&&which==bt&&(p-ws-1)==editpos) {
					cairo_set_source_rgb(cr,0.9,0.9,0.5);
					cairo_rectangle(cr,x,bt->y+2,*p,button_height+1);
					cairo_fill(cr);
				}
				cairo_set_source_rgb(cr,0,0,0);
				cairo_move_to (cr, x, y);
				cairo_show_text(cr,nms[*n++].s);
				cairo_stroke(cr);
				x+=(*p++);
			}
		} else {
				x+=5;
				cairo_move_to (cr, x, y);
				cairo_show_text(cr,ns);

				cairo_set_source_rgb(cr,0,0,0.5);
				char *p=(char *)(bt->n->def[0]);
				char *e=p+bt->n->def[1];
				p+=bt->pos;
				if((p+8)>=e) { p=e-8-1; }
				else { e=p+8; }
				
				char s[4]; while(p<e) { sprintf(s," %02hhx",*p++); cairo_show_text(cr,s); }

				cairo_stroke(cr);
		}
	}
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

