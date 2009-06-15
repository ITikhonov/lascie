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

struct tag { uint32_t x,y,w,h; char s[8]; uint8_t t; void *data; uint32_t l; uint8_t nospace; struct e *def; };

static struct e { struct e *n; struct tag *o; } editcode[1024];
static struct e *editcode_e=editcode;

struct voc { struct tag heads[256], *end; };

static struct voc commands = {.end=commands.heads};
static struct voc macros = {.end=macros.heads};
static struct voc words = {.end=words.heads};
static struct voc datas = {.end=datas.heads};

struct editor { struct tag *tag; struct e **pos; int x, y; } edit;

static struct e final={0,0};

struct tag *selected=0;

void draw();

static void do_exit() { exit(0); }

inline void saveword(struct tag *t, FILE *f) {
	fwrite(&t->t,1,1,f);
	fwrite(t->s,8,1,f);
	fwrite(&t->x,4,1,f);
	fwrite(&t->y,4,1,f);

	struct e *e=t->def;
	for(;e;e=e->n) {
		fwrite(&e->o->t,1,1,f);
		struct voc *v;
		switch(e->o->t) {
			case macro: v=&macros; break;
			case command: v=&commands; break;
			case data: v=&datas; break;
			case compiled: v=&words; break;
		}
		uint16_t n = e->o - v->heads;
		fwrite(&n,2,1,f);
	}
	fwrite("\xff",1,1,f);
}

static void do_save() {
	struct tag *t;
	FILE *f=fopen("save","w");
	for(t=datas.heads;t<datas.end;t++) { saveword(t,f); }
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

	datas.end=datas.heads;
	words.end=words.heads;

	for(;;) {
		uint8_t tp;
		struct tag *t;
		fread(&tp,1,1,f);
		if(tp==0xff) break;

		switch(tp) {
			case compiled: t=words.end++; break;
			case data: t=datas.end++; break;
			default: abort();
		}

		t->t=tp;
		fread(t->s,8,1,f);
		fread(&t->x,4,1,f);
		fread(&t->y,4,1,f);
		resize(t);

		struct e **p=&t->def;

		for(;;) {
			uint8_t tp;
			fread(&tp,1,1,f);
			if(tp==0xff) break;

			struct voc *v;
			switch(tp) {
				case macro: v=&macros; break;
				case command: v=&commands; break;
				case data: v=&datas; break;
				case compiled: v=&words; break;
			}

			uint16_t n;
			fread(&n,2,1,f);

			struct e *e=editcode_e++;
			e->o=v->heads+n;
			e->n=0;
			*p=e;
			p=&(e->n);
		}
	}
	fclose(f);
	draw();
}


int nospace=0;
static struct tag *add(int x, int y, char *s, void *f, int len, int t) {
	struct tag *c;
	switch(t) {
	case macro: c=macros.end++; break;
	case command: c=commands.end++; break;
	case data: c=datas.end++; break;
	default: c=words.end++;
	}

	c->x=x; c->y=y; c->t=t; c->data=f; c->l=len; c->nospace=nospace;
	strncpy(c->s,s,7);
	resize(c);
	c->def=(t==compiled||t==data)?&final:0;
	return c;
}

static void openeditor(struct tag *t) {
	edit.tag=t; edit.x=edit.tag->x; edit.y=edit.tag->y+button_height+5; edit.pos=&(edit.tag->def); draw();
}

static void do_create() { openeditor(add(100,100,"",0,0,compiled)); }
static void do_data() { openeditor(add(100,100,"",0,0,data)); draw(); }

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
static void compile_fetch();
static void compile_store();

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
	if(edit.tag && edit.tag->data) { execute(edit.tag->data); }
	else if(selected && selected->data) { execute(selected->data); }
	draw();
}


static void do_ping(void) { puts("PONG"); }
static void compile_dup();
static void compile_drop();
static void compile_dec();
static void compile_inc();

void init(cairo_t *cr1) {
	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;

	add(30,110,"data", do_data,0,command);

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

	add(60,310,"@", compile_fetch,0,macro);
	add(90,310,"!", compile_store,0,macro);

	add(220,310,"!?", compile_notif,0,macro);
	add(30,290,"ping", do_ping,0,command);
	add(30,310,"?", do_if,0,macro);
	add(30,330,"}", do_end,0,macro);
	add(60,330,"dup", compile_dup,0,macro);
	add(90,330,"drop", compile_drop,0,macro);
	add(130,330,"1-", compile_dec,0,macro);
	add(130,310,"1+", compile_inc,0,macro);
	add(160,330,"{", compile_begin,0,macro);
	add(190,330,"<<", compile_rewind,0,macro);

	add(30,250,"load", do_load,0,command);
	add(30,270,"save", do_save,0,command);

	add(30,90,"execute", do_execute,0,command);
	add(30,70,"compile", do_compile,0,command);
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);
	final.o=add(30,130,";",do_ret,0,macro);
}

float *padcolor;
float normalcolor[] ={0.5,0.9,0.5};
float macrocolor[]  ={0.9,0.5,0.9};
float commandcolor[]={0.9,0.5,0.5};
float datacolor[]   ={0.9,0.9,0.5};

inline void setpadcolor() { cairo_set_source_rgb(cr,padcolor[0],padcolor[1],padcolor[2]); }
inline void dullcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.8); }
inline void selectcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.0); }

inline void textcolor() { cairo_set_source_rgb(cr,0,0,0); }


static int x,y;

inline void label(struct tag *o) {
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


void drawtag(struct tag *t) {
	if(t==selected) selectcolor(); else setpadcolor();
	x=t->x; y=t->y;
	label(t);
}

void draweditor(struct editor *ed) {
	x=ed->x; y=ed->y;
	dullcolor();
	label(ed->tag);
	x+=ed->tag->w;

	struct e *e=ed->tag->def;
	if(!e) return;
	for(;e;e=e->n) {
		if(e==*edit.pos) selectcolor(); else dullcolor();
		label(e->o);
		x+=e->o->w;
	}
}

void draw() {
	cairo_set_source_rgb(cr,255,255,255);
	cairo_paint(cr);

	struct tag *e;
	for(e=macros.heads;e<macros.end;e++) { padcolor=macrocolor; drawtag(e); }
	for(e=datas.heads;e<datas.end;e++) { padcolor=datacolor; drawtag(e); }
	for(e=words.heads;e<words.end;e++) { padcolor=normalcolor; drawtag(e); }
	for(e=commands.heads;e<commands.end;e++) { padcolor=commandcolor; drawtag(e); }

	if(edit.tag) draweditor(&edit);

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

static void compile_fetch() {
	*cc.b++=0x8b;
	*cc.b++=0x00;
}

static void compile_store() {
	*cc.b++=0x89;
	*cc.b++=0xc2;
	compile_drop();
	*cc.b++=0x89;
	*cc.b++=0x02;
	compile_drop();
}

static void compile_dec() { *cc.b++=0x48; }
static void compile_inc() { *cc.b++=0x40; }

static void delay(struct tag *w) {
	dec->p=cc.i++; dec->w=w; dec++;
}

inline void *compiledata(struct tag *t) {
	void *beginning=cc.b;
	fwjump=fwjumps;
	bwjump=bwjumps;
	struct e *e=t->def;
	for(;e;e=e->n) {
		if(e->o->t == macro) { void (*f)(struct e *e) = e->o->data; f(e); }
	}
	cc.b=beginning;
	return beginning;
}

inline void compilelist(struct tag *t) {
	t->data=cc.b;
	fwjump=fwjumps;
	bwjump=bwjumps;

	struct e *e=t->def;
	for(;e;e=e->n) {
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
			if(e->o->data == 0 || t<e->o) delay(e->o);
			else { *cc.i++=((uint8_t*)e->o->data)-(cc.b+4); }
			if(e->o->t==command) { compile_drop(); }
		}
	}
}

static void do_compile() {
	cc.b=ccode;
	struct tag *e;
	dec=decs;
	for(e=words.heads;e<words.end;e++) { compilelist(e); }
	for(e=datas.heads;e<datas.end;e++) { execute(compiledata(e)); e->data=realloc(e->data,*stack++); }
	while(--dec>=decs) {
		*dec->p=dec->w->t==data?((uint32_t)dec->w->data) : ((uint8_t *)dec->w->data-(uint8_t*)(dec->p+1));
	}
}

inline int clickcommand(struct tag *e, int x1,int y1) {
	if(!(e->y<=y1 && y1<=e->y+e->h && x1>e->x && x1<e->x+e->w)) return 0;
	void (*f)(void)=(void *)e->data; f(); return 1;
}


inline int clicktag(struct tag *t, int x1,int y1) {
	if(!(t->y<=y1 && y1<=t->y+t->h && x1>t->x && x1<t->x+t->w)) return 0;
	if(edit.tag) {
		struct e *e=editcode_e++;
		e->o=t;
		e->n=*edit.pos;
		*edit.pos=e;
		edit.pos=&e->n;
	} else if((t->t==data || t->t==compiled) && t==selected) {
		openeditor(t);
	} else {
		selected=t;
	}
	draw();
	return 1;
}

inline int clickeditor(struct editor *ed, int x1, int y1) {
	if(!(ed->y<=y1 && y1<=ed->y+button_height+5 && x1>ed->x)) return 0;

	x=ed->x+ed->tag->w; y=ed->y;
	if(x1<=x) { edit.tag=0; draw(); return 1; }

	struct e **p=&ed->tag->def;
	for(;*p;p=&(*p)->n) {
		x+=(*p)->o->w;
		if(x1<=x) { ed->pos=p; draw(); return 1; }
	}
	return 0;
}

void release(int x1,int y1) {
	struct tag *e;
	for(e=commands.heads;e<commands.end;e++) { if(clickcommand(e,x1,y1)) return; }

	for(e=words.heads;e<words.end;e++) { if(clicktag(e,x1,y1)) return; }
	for(e=datas.heads;e<datas.end;e++) { if(clicktag(e,x1,y1)) return; }
	for(e=macros.heads;e<macros.end;e++) { if(clicktag(e,x1,y1)) return; }

	if(edit.tag) { if(!clickeditor(&edit,x1,y1)) {edit.x=x1 & 0xfffffff0; edit.y=y1 & 0xfffffff0; draw(); return;} }
	else if(selected) { selected->x=x1 & 0xfffffff0; selected->y=y1 & 0xfffffff0; draw(); return; }

	draw();
}

void button(int x1, int y1) {
}

void key(int k) {
	if(edit.tag) {
		switch(k) {
			case XK_BackSpace:
				edit.tag->s[strlen(edit.tag->s)-1]=0;
				resize(edit.tag);
				draw();
				break;
			case XK_Delete:
				if((*edit.pos)->n) { *(edit.pos)=(*edit.pos)->n; draw(); } break;
			case XK_Return:
				edit.tag=0; draw(); break;
			case '0'...'9':
			case 'a'...'z': {
				char *s=edit.tag->s;
				int l=strlen(s);
				s[l]=k;
				s[l+1]=0;
				resize(edit.tag);
				draw();
				}
		}
	}
}

