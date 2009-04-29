#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

static cairo_t *cr=0;
int button_height=0;

struct nm { char s[8]; union { uint8_t *b; int *i; } def; int len, data; } nms[100];
struct nm *nme=nms;
struct nm *nmu;

struct bt { int x,y; struct nm *n; void (*act)(void); void (*draw)(struct bt *); int x2,y2,pos;} bts[100];
struct bt *bte=bts;
struct bt *btu;

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
void draw_data(struct bt *);
void draw_code(struct bt *);
void draw();

void hit(int x,int y) {
	struct bt *p=bte; for(;p!=bts;) { --p; if(x>p->x&&x<p->x2 && y>p->y&&y<p->y2) {clicked=p; return;} }
	clicked=0;
}


void do_choose() { which=clicked; draw(); }
void do_exit() { exit(0); }
void do_move() { if(which) { ops=&move1; }; }
void do_rename() { if(which) { pos=0; which->n->s[0]=0; ops=&rename1; draw(); } }
void do_open() {
	if(!which) return;
	struct bt *w=bte++;
	w->x=which->x;
	w->y=which->y2;
	w->n=which->n;
	w->act=do_choose;
	w->draw=which->n->data?draw_data:draw_code;
	which=w;
	draw();
}

void do_close() {
	if(!which) return;
	memmove(which,--bte,sizeof(struct bt));
	which=0;
	draw();
}

void do_load() {
	int f=open("save",O_RDONLY);
	int n;
	nme=nmu;
	bte=btu;

	read(f,&n,4);
	while(n--) {
		struct bt *p=bte++;
		int x;
		read(f,&p->x,4);
		read(f,&p->y,4);
		read(f,&x,4);
		p->n=nmu+x;
		p->act=do_choose;
		p->draw=draw_button;
	}

	read(f,&n,4);
	while(n--) {
		struct nm *p=nme++;
		read(f,&p->s,8);
		read(f,&p->len,4);
		p->def.b=malloc(p->len);
		read(f,p->def.b,p->len);
		read(f,&p->data,4);
	}
	draw();
}

void do_save() {
	int f=open("save",O_WRONLY|O_CREAT|O_TRUNC,0644);
	int x=bte-btu;
	write(f,&x,4);
	struct bt *p=btu; for(;p<bte;p++) {
		write(f,&p->x,4);
		write(f,&p->y,4);
		x=p->n-nmu;
		write(f,&x,4);
	}
	x=nme-nmu;
	write(f,&x,4);
	struct nm *r=nmu; for(;r<nme;r++) {
		write(f,&r->s,8);
		write(f,&r->len,4);
		write(f,r->def.b,r->len);
		write(f,&r->data,4);
	}
	close(f);
}

int editpos=-1;

void do_edit() {
	if(!which) return;
	editpos=0;
	ops=&edit;
	draw();
}

void do_down() { if(which) { if((which->pos+8)<which->n->len) { which->pos+=8; draw(); } } }
void do_up() { if(which) { if((which->pos-8)>=0) { which->pos-=8; draw(); } } }

static uint8_t *ccode;


uint8_t *compile_at;


uint8_t *nests[5];
uint8_t **cnest;

void do_nest() { }

void do_question() {
	*compile_at++=0x75;
	*cnest++=compile_at;
	*compile_at++=0x00; // placeholder for unnest
	
}

void do_unnest() {
	--cnest;
	int off=((uint32_t)compile_at)-((uint32_t)(*cnest+1));
	**cnest=off;
}

void do_compile() {
	int32_t *i;
	uint8_t *c,*ca;
	struct nm *p;

	cnest=nests;

	c=ccode+5*(nme-nmu);
	ca=ccode;
	p=nmu; for(;p<nme;p++) {
		*ca++=0xe9;
		*((int32_t *)ca)=(int32_t)c-(int32_t)(ca+4);
		ca+=4;

		i=p->def.i;
		for(;i<p->def.i+p->len/4;i++) {
			switch(nmu[*i].data) {
			case 2: {
					void (*f)(void)=(void*)nmu[*i].def.b;
					compile_at=c; f(); c=compile_at;
				} break;
			case 0:
				*c=0xe8; c++;
				if(*i<0) { *((int32_t *)c)=(int32_t)(nmu[*i].def.b)-(int32_t)(c+4); }
				else { *((int32_t *)c)=(int32_t)(ccode+5*(*i))-(int32_t)(c+4); }
				c+=4;
				break;
			}
		}
		*c=0xc3; c++;
	}
}

void do_run() {
	if(which) {
		void (*f)(void)=(void *)ccode+5*(which->n-nmu);
		f();
	}
}

void button_edit(int x, int y) {
	hit(x,y); if(clicked) {
		if((editpos+1)*4>which->n->len) {
			which->n->len+=4;
			which->n->def.i=realloc(which->n->def.i,which->n->len);
		}
		which->n->def.i[editpos++]=clicked->n-nmu;
		draw();
	} else { ops=&choose; }
}
void button_choose(int x,int y) { hit(x,y); if(clicked) clicked->act(); }
void button_move1(int x,int y) { which->x=x&(~0x7); which->y=y&(~0x7); ops=&choose; draw(); }

void do_create() {
	which=bte; which->n=nme; bte++; nme++;
	which->x=bts[0].x2+5; which->y=bts[0].y; which->n->s[0]=0; which->act=do_choose;
	which->n->def.i=0;
	which->n->len=0;
	which->draw=draw_button;
	draw();
	do_rename();
}

void key_rename1(int k) {
	if(k==XK_Return) { ops=&choose; return; }
	if(k==XK_BackSpace) { if(pos>0) { which->n->s[--pos]='\0'; draw(); } return; }
	if(pos<8-1) { which->n->s[pos++]='a'+(k-XK_a); which->n->s[pos]='\0'; }
	which->draw(which);
}

void add(int x, int y, char *s, void (*f)(void)) {
	struct bt *w=bte; w->n=nme; bte++; nme++;
	w->x=x; w->y=y; strncpy(w->n->s,s,8); w->act=f;
	w->n->def.b=0;
	w->n->len=0;
	w->n->data=0;
	w->draw = draw_button;
}

int8_t hexext[256];

void do_ping(void) { puts("PONG"); }

void init(cairo_t *cr1) {
	add(30,310,"run", do_run);

	add(140,30,"?", do_ping);
	ccode=bte[-1].n->def.b=(uint8_t*)do_question;
	bte[-1].n->data=2;
	bte[-1].n->len=0;

	add(100,30,"{", do_ping);
	ccode=bte[-1].n->def.b=(uint8_t*)do_nest;
	bte[-1].n->data=2;
	bte[-1].n->len=0;

	add(120,30,"}", do_ping);
	ccode=bte[-1].n->def.b=(uint8_t*)do_unnest;
	bte[-1].n->data=2;
	bte[-1].n->len=0;

	add(30,290,"ping", do_ping);
	ccode=bte[-1].n->def.b=(uint8_t*)do_ping;
	bte[-1].n->len=0;

	add(30,270,"code", do_choose);
	ccode=bte[-1].n->def.b=(uint8_t *)malloc(65536);
	bte[-1].n->len=65536;
	bte[-1].n->data=1;
	bte[-1].pos=0;

	add(30,250,"compile", do_compile);
	add(30,230,"load", do_load);
	add(30,210,"save", do_save);
	add(30,190,"close", do_close);
	add(30,170,"down", do_down);
	add(30,150,"up", do_up);
	add(30,130,"edit", do_edit);
	add(30,110,"open", do_open);
	add(30,90,"rename", do_rename);
	add(30,70,"move", do_move);
	add(30,50,"exit", do_exit);
	add(30,30,"create", do_create);

	btu=bte;
	nmu=nme;

#if 0
	add(90,90,"test", do_choose);
	bte[-1].n->def.i=(int *)malloc(16);
	bte[-1].n->len=16;
	bte[-1].n->def.i[0]=0;
	bte[-1].n->def.i[1]=1;
	bte[-1].n->def.i[2]=-1;
	bte[-1].n->def.i[3]=-2;

	add(150,90,"dtest", do_choose);
	int8_t *p=bte[-1].n->def.b=(int8_t *)malloc(17);
	{ int i; for(i=0;i<17;i++) p[i]=i; }
	bte[-1].n->len=17;
	bte[-1].n->data=1;
	bte[-1].pos=8;
#endif

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
	cairo_text_extents_t te;
	cairo_text_extents(cr,s,&te);

	bt->x2=bt->x+5+te.width+5;
	bt->y2=bt->y+button_height+5;
	int x=bt->x+5,y=bt->y+button_height;

	if(bt==which) { cairo_set_source_rgb(cr,0.9,0.5,0.5); } else { cairo_set_source_rgb(cr,0.5,0.9,0.5); }
	cairo_rectangle(cr,bt->x,bt->y,bt->x2-bt->x,bt->y2-bt->y);
	cairo_fill(cr);

	cairo_set_source_rgb(cr,0,0,0);
	cairo_move_to (cr, x, y);
	cairo_show_text (cr, s);
	cairo_stroke(cr);
}

void draw_code(struct bt *bt) {
	char *s=bt->n->s;
	int ws[11], wn=1;

	cairo_text_extents_t te;
	cairo_text_extents(cr,s,&te);
	ws[0]=te.width;

	int *n=bt->n->def.i;
	int *e=n+bt->n->len/4;
	int *p=ws+1;
	while(n<e) {
		cairo_text_extents(cr,nmu[*n++].s,&te);
		*p++=te.width;
		wn++;
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

	x+=ws[0];

	n=bt->n->def.i;
	e=n+bt->n->len/4;
	p=ws+1;
	while(n<e) {
		x+=5;
		if(ops==&edit&&which==bt&&(p-ws-1)==editpos) {
			cairo_set_source_rgb(cr,0.9,0.9,0.5);
			cairo_rectangle(cr,x,bt->y+2,*p,button_height+1);
			cairo_fill(cr);
		}
		cairo_set_source_rgb(cr,0,0,0);
		cairo_move_to (cr, x, y);
		cairo_show_text(cr,nmu[*n++].s);
		cairo_stroke(cr);
		x+=(*p++);
	}
}

void draw_data(struct bt *bt) {
	char *s=bt->n->s;
	int ws[11], wn=1;
	char ns[10];

	cairo_text_extents_t te;
	cairo_text_extents(cr,s,&te);
	ws[0]=te.width;

	sprintf(ns,"%x:",bt->n->len);
	cairo_text_extents(cr,ns,&te);
	ws[1]=te.x_advance;
	wn++;

	uint8_t *p=bt->n->def.b+bt->pos;
	int m=bt->n->len;
	if(m>8) m=8;
	uint8_t *e=p+m;
	ws[2]=0;
	wn++;
	while(p<e) { ws[2]+=hexext[*p++]; }

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

	x+=ws[0];
	x+=5;
	cairo_move_to (cr, x, y);
	cairo_show_text(cr,ns);

	cairo_set_source_rgb(cr,0,0,0.5);
	p=bt->n->def.b;
	e=p+bt->n->len;
	p+=bt->pos;
	if((p+8)>=e) { p=e-8-1; }
	else { e=p+8; }
	
	char h[4]; while(p<e) { sprintf(h," %02hhx",*p++); cairo_show_text(cr,h); }

	cairo_stroke(cr);
}

void draw() {
	cairo_save(cr);
	cairo_set_source_rgb(cr,255,255,255);
	cairo_paint(cr);
	struct bt *p=bts; for(;p<bte;p++) { p->draw(p); }
	cairo_restore(cr);
}

void button(int x,int y) { if(ops->button) ops->button(x,y); }
void key(int k)		 { if(ops->key) ops->key(k); }

