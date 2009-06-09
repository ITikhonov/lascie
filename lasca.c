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
static int button_height=0;

enum nmflag { compiled, data, macro, command };

struct tag { uint32_t x,y,w,h; char s[8]; uint8_t t; void *data; uint32_t l; uint8_t nospace; };

static struct tag tags[256], *tags_e = tags;
static struct tag *user=0;
static struct e { struct e *n; struct tag *o; } editcode[1024];
static struct e *editcode_e=editcode;

struct voc { struct e heads[256], *end; };

struct voc macros = {.end=macros.heads};

static struct e heads[256], *heads_e = heads;

static struct e *userh=0;

static struct e *final=0;

void draw();

static void do_exit() { exit(0); }

inline void savelist(struct e *e, FILE *f) {
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

static void do_save() {
	struct e *e;
	FILE *f=fopen("save","w");
	for(e=heads;e<heads_e;e++) { savelist(e,f); }
	fclose(f);
}

static void resize(struct tag *c) {
	cairo_text_extents_t te;
	cairo_text_extents(cr,c->s,&te);
	c->w=te.x_advance+(c->nospace?0:10); c->h=button_height+5;
}


static void do_load() {
}


int nospace=0;
static struct e *add(int x, int y, char *s, void *f, int len, int t) {
	struct tag *c=tags_e++;
	c->x=x; c->y=y; c->t=t; c->data=f; c->l=len; c->nospace=nospace;
	strncpy(c->s,s,7);
	resize(c);

	struct e *h;
	switch(t) {
	case macro: h=macros.end++; break;
	default: h=heads_e++;
	}
	h->o=c;
	h->n=t==compiled?final:0;
	return h;
}

static struct e *editp=0;
static struct e *edit=0;

static void do_create() { editp=0; edit=add(100,100,"",0,0,compiled); draw(); }

static void do_compile();
static void do_ret(struct e *e);
static void do_if(struct e *e);
static void compile_notif(struct e *e);
static void do_end(struct e *e);
static void compile_begin(struct e *e);
static void compile_rewind(struct e *e);

void compile_neg(struct e *e);
static void compile_0(struct e *e);
static void compile_1(struct e *e);
static void compile_2(struct e *e);
static void compile_3(struct e *e);
static void compile_4(struct e *e);
static void compile_5(struct e *e);
static void compile_6(struct e *e);
static void compile_7(struct e *e);
static void compile_8(struct e *e);
static void compile_9(struct e *e);
static void compile_d(struct e *e);
static void compile_data(struct e *e);

uint32_t stackh[32]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}, *stack=stackh+30;

static void execute(void (*f)(void)) {
	asm volatile (
		"movl stack,%%esi; lodsl;"
		"call *%%edx;"
		"leal -4(%%esi),%%esi;"
		"movl %%eax,(%%esi);"
		"movl %%esi,stack"
		: : "d" (f) : "esi","eax","memory"
	);
}

static void do_execute() {
	if(edit && edit->o->data) { execute(edit->o->data); }
	draw();
}


static void do_ping(void) { puts("PONG"); }
static void compile_dup();
static void compile_drop();
static void compile_dec();

static int8_t hexext;

void init(cairo_t *cr1) {
	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;
	resize(tags);

	int n;
	char s[2]={0,0};
	for(n='0';n<='9';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }
	for(n='a';n<='f';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }

	add(50,310,"data", compile_data,0,macro);

	nospace=1;

	add(300,310,"d", compile_d,0,macro);
	add(300,290,"0", compile_0,0,macro);
	add(300,330,"-", compile_neg,0,macro);

	add(320,290,"1", compile_1,0,macro);
	add(335,290,"2", compile_2,0,macro);
	add(350,290,"3", compile_3,0,macro);

	add(320,310,"4", compile_4,0,macro);
	add(335,310,"5", compile_5,0,macro);
	add(350,310,"6", compile_6,0,macro);

	add(320,330,"7", compile_7,0,macro);
	add(335,330,"8", compile_8,0,macro);
	add(350,330,"9", compile_9,0,macro);
	nospace=0;


	add(220,310,"!?", compile_notif,0,macro);
	add(30,290,"ping", do_ping,0,command);
	add(30,310,"?", do_if,0,macro);
	add(30,330,"}", do_end,0,macro);
	add(60,330,"dup", compile_dup,0,macro);
	add(90,330,"drop", compile_drop,0,macro);
	add(130,330,"1-", compile_dec,0,macro);
	add(160,330,"{", compile_begin,0,macro);
	add(190,330,"<<", compile_rewind,0,macro);

	add(30,250,"load", do_load,0,command);
	add(30,270,"save", do_save,0,command);

	add(30,110,"ph", do_ping,0,command);
	add(30,90,"execute", do_execute,0,command);
	add(30,70,"compile", do_compile,0,command);
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);
	final=add(30,130,";",do_ret,0,macro);
	user=tags_e;
	userh=heads_e;
}

static int dull=0;

inline void padcolor() { if(!dull) { cairo_set_source_rgb(cr,0.5,0.9,0.5); } else { cairo_set_source_rgb(cr,0.8,0.8,0.8); } }
inline void textcolor() { cairo_set_source_rgb(cr,0,0,0); }
inline void macrocolor() { cairo_set_source_rgb(cr,0.9,0.5,0.9); }
inline void commandcolor() { cairo_set_source_rgb(cr,0.9,0.5,0.5); }
inline void datacolor() { cairo_set_source_rgb(cr,0.9,0.9,0.5); }

static int x,y;

inline void label(struct tag *o) {
	if(dull) padcolor();
	else {
		switch(o->t) {
		case compiled:	padcolor(); break;
		case macro:	macrocolor(); break;
		case data:	datacolor(); break;
		case command:	commandcolor(); break;
		}
	}

	cairo_rectangle(cr,x,y,o->w,o->h);
	cairo_fill(cr);

	textcolor();
	
	cairo_move_to(cr, x+(o->nospace?0:5), y+button_height);
	cairo_show_text(cr, o->s);
	cairo_stroke(cr);

}

void drawstack() {
	char s[10];
	cairo_move_to(cr, 5, 5+button_height);
	uint32_t *p=stack;

	textcolor();
	while(p<stackh+32) {
		sprintf(s,"%x ",*p);
		cairo_show_text(cr, s);
		p++;
	}
	cairo_stroke(cr);
}

inline void drawlist(struct e *e) {
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
	for(e=macros.heads;e<macros.end;e++) { drawlist(e); }
	for(e=heads;e<heads_e;e++) { drawlist(e); }

	drawstack();
}

static union ic { uint8_t *b; int8_t *c; int32_t *i; void *v; } cc;
static uint8_t ccode[65535];
static struct { int32_t *p; struct tag *w; } decs[1024], *dec=decs;

static void compile_imm(int32_t x) { *cc.b++=0xb8; *cc.i++=x; }

int n=0, n_sign=1;
void nend(struct e *e) { if(!e->n->o->nospace) compile_d(e); }
void compile_neg(struct e *e) { n_sign=-1; nend(e); }
void compile_0(struct e *e) { n=n*10; nend(e);}
void compile_1(struct e *e) { n=n*10+1; nend(e); }
void compile_2(struct e *e) { n=n*10+2; nend(e); }
void compile_3(struct e *e) { n=n*10+3; nend(e); }
void compile_4(struct e *e) { n=n*10+4; nend(e); }
void compile_5(struct e *e) { n=n*10+5; nend(e); }
void compile_6(struct e *e) { n=n*10+6; nend(e); }
void compile_7(struct e *e) { n=n*10+7; nend(e); }
void compile_8(struct e *e) { n=n*10+8; nend(e); }
void compile_9(struct e *e) { n=n*10+9; nend(e); }
void compile_d(struct e *e) { compile_dup(); compile_imm(n_sign*n); n=0; n_sign=1; }

struct e *compilednow=0;

void do_data() {
	register uint32_t *stack asm("esi");
	printf("alloc: %u %u\n",stack[0],stack[1]);
	compilednow->o->data=realloc((void *)stack[0],stack[1]);
	printf(" -> %u\n",(uint32_t)(compilednow->o->data));
}

void compile_data(struct e *e) {
	compilednow->o->t=data;
	compile_dup();
	compile_imm((int32_t)compilednow->o->data);
	compile_dup();
	*cc.b++=0xe8;
	*cc.i++=((uint8_t*)do_data)-(cc.b+4);
	compile_drop();
	compile_drop();
	compile_drop();
}

static void do_ret(struct e *e) { *cc.b++=0xc3; }
int8_t *fwjumps[8], **fwjump;
static void do_if(struct e *e) {
	*cc.b++=0x75;
	*fwjump++=cc.c++;
}
static void compile_notif(struct e *e) {
	*cc.b++=0x74;
	*fwjump++=cc.c++;
}
static void do_end(struct e *e) {
	--fwjump;
	**fwjump = cc.b - (uint8_t*)((*fwjump)+1);
}

int8_t *bwjumps[8], **bwjump;
static void compile_begin(struct e *e) {
	*bwjump++=cc.c;
}
static void compile_rewind(struct e *e) {
	--bwjump;
	*cc.b++=0xeb;
	*cc.c++=*bwjump-(cc.c+1);
}

static void compile_dup() {
	*cc.b++=0x8d; *cc.b++=0x76; *cc.b++=0xfc;
	*cc.b++=0x89; *cc.b++=0x06;
}

static void compile_drop() {
	*cc.b++=0xad;
}

static void compile_dec() {
	*cc.b++=0x48;
}

static void delay(struct tag *w) {
	dec->p=cc.i++; dec->w=w; dec++;
}

inline void compilelist(struct e *e) {
	compilednow=e;
	void *beginning=cc.b;
	fwjump=fwjumps;
	bwjump=bwjumps;
	for(e=e->n;e;e=e->n) {
		switch(e->o->t) {
		case macro: { void (*f)(struct e *e) = e->o->data; f(e); } break;
		case data:
			compile_dup();
			*cc.b++=0xb8;
			delay(e->o);
			break;
		default:
			if(e->o->t==command) { compile_dup(); }
			*cc.b++=0xe8;
			if(e->o->data == 0 || compilednow->o<e->o) delay(e->o);
			else { *cc.i++=((uint8_t*)e->o->data)-(cc.b+4); }
			if(e->o->t==command) { compile_drop(); }
		}
	}
	compilednow->o->data=beginning;
}

static void do_compile() {
	cc.b=ccode;
	struct e *e;
	dec=decs;
	for(e=heads;e<heads_e;e++) {
		if(e->o->t==compiled||e->o->t==data) { compilelist(e); }
	}
	for(e=heads;e<heads_e;e++) {
		if(e->o->t==data) { compilednow=e; execute(e->o->data); }
	}
	while(--dec>=decs) {
		*dec->p=dec->w->t==data?((uint32_t)dec->w->data) : ((uint8_t *)dec->w->data-(uint8_t*)(dec->p+1));
	}
}


inline int clicklist(struct e *e, int x1,int y1) {
	if(!(e->o->y<=y1 && y1<=e->o->y+e->o->h && x1>e->o->x)) return 0;
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
	for(e=macros.heads;e<macros.end;e++) { if(clicklist(e,x1,y1)) return; }
	for(e=heads;e<heads_e;e++) { if(clicklist(e,x1,y1)) return; }

	if(edit&&!editp) { edit->o->x=x1 & 0xfffffff0; edit->o->y=y1 & 0xfffffff0; draw(); return;}

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
					if(edit->n) { editp->n=edit->n; edit=edit->n; draw(); } break;
			}
		}
	}
}

