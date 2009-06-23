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

#define NDEBUG
#include <assert.h>

int max(int x,int y) { return x>y?x:y; }
int min(int x,int y) { return x>y?y:x; }

static cairo_t *cr=0;
static int button_height=0;
uint8_t gen=0;

enum nmflag { compiled=0, data=1, macro=2, command=3, builtin=4 };

struct tag { uint32_t x,y,w,h; char s[8]; uint8_t t; void *data; uint32_t l; uint8_t nospace; struct e *def; uint8_t gen; };

static struct e { struct e *n; enum nmflag t; struct tag *o; } editcode[1024];
static struct e *editcode_e=editcode;

struct voc { struct tag heads[256], *end; };

static struct voc commands = {.end=commands.heads};
static struct voc builtins = {.end=builtins.heads};
static struct voc words = {.end=words.heads};

struct editor { struct tag *tag; struct e **pos; int x, y; } edit;

static struct e final={.n=0,.t=builtin};

struct tag *selected=0;

void draw();

static void do_exit() { exit(0); }

inline void saveword(struct tag *t, FILE *f) {
	fwrite(&t->t,1,1,f);
	fwrite(&t->x,4,1,f);
	fwrite(&t->y,4,1,f);
	fwrite(t->s,8,1,f);

	struct e *e=t->def;
	for(;e;e=e->n) {
		fwrite(&e->o->t,1,1,f);
		struct voc *v;
		switch(e->o->t) {
			case builtin: v=&builtins; break;
			default: v=&words; break;
		}
		uint16_t n = e->o - v->heads;
		fwrite(&n,2,1,f);
	}
	fwrite("\xff",1,1,f);
}

static void do_save() {
	struct tag *t;
	FILE *f=fopen("save","w");
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

	words.end=words.heads;

	for(;;) {
		uint8_t tp;
		fread(&tp,1,1,f);
		if(tp==0xff) break;

		struct tag *t=words.end++;

		t->t=tp;
		fread(&t->x,4,1,f);
		fread(&t->y,4,1,f);
		fread(t->s,8,1,f);
		printf("name %s\n",t->s);
		resize(t);
		t->gen=gen;

		struct e **p=&t->def;

		for(;;) {
			uint8_t tp;
			fread(&tp,1,1,f);
			printf("  type %x\n",tp);
			if(tp==0xff) break;

			struct voc *v;
			switch(tp) {
				case builtin: v=&builtins; break;
				default: v=&words; break;
			}

			uint16_t n;
			fread(&n,2,1,f);
			printf("  n    %d\n",n);

			struct e *e=editcode_e++;
			e->o=v->heads+n;
			e->t=tp;
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
	case builtin: c=builtins.end++; break;
	case command: c=commands.end++; break;
	default: c=words.end++;
	}

	c->x=x; c->y=y; c->t=t; c->data=f; c->l=len; c->nospace=nospace; c->gen=gen;
	strncpy(c->s,s,7);
	resize(c);
	c->def=t==compiled?&final:0;
	return c;
}

static void openeditor(struct tag *t) {
	edit.tag=t; edit.x=edit.tag->x; edit.y=edit.tag->y+button_height+5; edit.pos=&(edit.tag->def); draw();
}

static void do_create() { openeditor(add(100,100,"",0,0,compiled)); }

static void do_compile();
static void do_plan();
static void do_ret();
static void do_if();
static void compile_notif();
static void compile_ifns();
static void do_end();
static void compile_begin();
static void compile_rewind();

void compile_neg();
static void compile_0();
static void compile_1();
static void compile_2();
static void compile_3();
static void compile_4();
static void compile_5();
static void compile_6();
static void compile_7();
static void compile_8();
static void compile_9();
static void compile_a();
static void compile_b();
static void compile_c();
static void compile_d();
static void compile_e();
static void compile_f();
static void compile_n();
static void compile_h();
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

inline void change_type(enum nmflag p) {
	if(edit.tag) (*edit.pos)->t=p;
	else if(selected) selected->t=p;
	draw();
}
static void do_macro() { change_type(macro); }
static void do_normal() { change_type(compiled); }
static void do_data() { change_type(data); }


static void do_ping(void) { puts("PONG"); }
static void compile_dup();
static void compile_drop();
static void compile_dec();
static void compile_inc();
static void compile_nip();
static void compile_add();
static void compile_sub();
static void compile_over();
static void compile_swap();
static void compile_spot();
static void compile_h();
static void compile_allot();

void init(cairo_t *cr1) {
	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	cairo_text_extents_t te;
	cairo_text_extents(cr,"abcdefghijklmnopqrstuvwxyz0123456789;",&te);
	button_height=te.height;

	nospace=1;

	add(285,310,"h", compile_h,0,builtin);
	add(300,310,"d", compile_n,0,builtin);
	add(300,290,"0", compile_0,0,builtin);
	add(300,330,"-", compile_neg,0,builtin);

	add(320,290,"1", compile_1,0,builtin);
	add(335,290,"2", compile_2,0,builtin);
	add(350,290,"3", compile_3,0,builtin);

	add(320,310,"4", compile_4,0,builtin);
	add(335,310,"5", compile_5,0,builtin);
	add(350,310,"6", compile_6,0,builtin);

	add(320,330,"7", compile_7,0,builtin);
	add(335,330,"8", compile_8,0,builtin);
	add(350,330,"9", compile_9,0,builtin);

	add(365,290,"a", compile_a,0,builtin);
	add(365,310,"b", compile_b,0,builtin);
	add(365,330,"c", compile_c,0,builtin);

	add(380,290,"d", compile_d,0,builtin);
	add(380,310,"e", compile_e,0,builtin);
	add(380,330,"f", compile_f,0,builtin);
	nospace=0;

	add(60,310,"@", compile_fetch,0,builtin);
	add(90,310,"!", compile_store,0,builtin);

	add(220,310,"!?", compile_notif,0,builtin);
	add(30,290,"ping", do_ping,0,command);
	add(30,310,"?", do_if,0,builtin);
	add(30,330,"}", do_end,0,builtin);
	add(60,330,"dup", compile_dup,0,builtin);
	add(90,330,"drop", compile_drop,0,builtin);
	add(130,330,"1-", compile_dec,0,builtin);
	add(130,310,"1+", compile_inc,0,builtin);
	add(160,330,"{", compile_begin,0,builtin);
	add(190,330,"<<", compile_rewind,0,builtin);

	add(30,250,"load", do_load,0,command);
	add(30,270,"save", do_save,0,command);

	add(30,90,"execute", do_execute,0,command);
	add(30,70,"compile", do_compile,0,command);
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);
	final.o=add(30,130,";",do_ret,0,builtin);
	add(60,290,"nip", compile_nip,0,builtin);
	add(90,290,"+", compile_add,0,builtin);
	add(120,290,"over", compile_over,0,builtin);
	add(150,290,"swap", compile_swap,0,builtin);

	add(80,30,"go",0,0,compiled);
	add(80,50,"spot",compile_spot,0,builtin);
	add(240,320,"?+", compile_ifns,0,builtin);
	add(90,270,"-", compile_sub,0,builtin);

	add(30,150,"macro",do_macro,0,command);
	add(30,170,"normal",do_normal,0,command);
	add(30,190,"data",do_data,0,command);
	add(30,210,"plan",do_plan,0,command);
	add(120,270,"allot", compile_allot,0,builtin);
}

void normalcolor()  {cairo_set_source_rgb(cr,0.5,0.9,0.5);}
void builtincolor() {cairo_set_source_rgb(cr,0.9,0.5,0.9);}
void macrocolor()   {cairo_set_source_rgb(cr,0.5,0.5,0.9);}
void datacolor()    {cairo_set_source_rgb(cr,0.9,0.9,0.5);}
void commandcolor() {cairo_set_source_rgb(cr,0.9,0.5,0.5);}

inline void dullcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.8); }
inline void selectcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.0); }

inline void textcolor() { cairo_set_source_rgb(cr,0,0,0); }

static int x,y;

inline void pad(struct tag *o) {
	cairo_rectangle(cr,x,y,o->w,o->h);
	cairo_fill(cr);
}

inline void text(struct tag *o) {
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

void typecolor(enum nmflag t) {
	switch(t) {
		case builtin: builtincolor(); break;
		case command: commandcolor(); break;
		case compiled: normalcolor(); break;
		case macro: macrocolor(); break;
		case data: datacolor(); break;
	}
}

void drawtag(struct tag *t) {
	x=t->x; y=t->y;
	if(t==selected) selectcolor();
	else typecolor(t->t);
	pad(t);
	textcolor(); text(t);
}


void draweditor(struct editor *ed) {
	x=ed->x; y=ed->y;
	dullcolor(); pad(ed->tag);
	textcolor(); text(ed->tag);
	x+=ed->tag->w;

	struct e *e=ed->tag->def;
	if(!e) return;
	for(;e;e=e->n) {
		if(e==*edit.pos) y-=button_height/4;
		typecolor(e->t);
		pad(e->o);
		textcolor(); text(e->o);
		if(e==*edit.pos) y+=button_height/4;
		x+=e->o->w;
	}
}

void draw() {
	cairo_set_source_rgb(cr,255,255,255);
	cairo_paint(cr);

	struct tag *e;
	for(e=builtins.heads;e<builtins.end;e++) { builtincolor(); drawtag(e); }
	for(e=words.heads;e<words.end;e++) { normalcolor(); drawtag(e); }
	for(e=commands.heads;e<commands.end;e++) { commandcolor(); drawtag(e); }

	if(edit.tag) draweditor(&edit);

	drawstack();
}

static union ic { uint8_t *b; int8_t *c; int32_t *i; void *v; } cc;
static uint8_t ccode[65535];
static struct { int32_t *p; struct tag *w; } decs[1024], *dec=decs;

static void compile_imm(int32_t x) { *cc.b++=0xb8; *cc.i++=x; }


int n=0, n_sign=1, base=10;
void compile_neg() { n_sign=-1; }
void compile_0() { n=n*base; }
void compile_1() { n=n*base+1; }
void compile_2() { n=n*base+2; }
void compile_3() { n=n*base+3; }
void compile_4() { n=n*base+4; }
void compile_5() { n=n*base+5; }
void compile_6() { n=n*base+6; }
void compile_7() { n=n*base+7; }
void compile_8() { n=n*base+8; }
void compile_9() { n=n*base+9; }
void compile_n() { compile_dup(); compile_imm(n_sign*n); n=0; n_sign=1; base=10; }

static void convert_h() {
	if(base==16) return;
	int x=n%10; n/=10;
	x+=(n%10)*0x10; n/=10;
	x+=(n%10)*0x100; n/=10;
	x+=(n%10)*0x1000; n/=10;
	x+=(n%10)*0x10000; n/=10;
	x+=(n%10)*0x100000; n/=10;
	x+=(n%10)*0x1000000; n/=10;
	x+=(n%10)*0x10000000; n/=10;
	n=x;
	base=16;
}

void compile_a() { convert_h(); n=n*16+10; }
void compile_b() { convert_h(); n=n*16+11; }
void compile_c() { convert_h(); n=n*16+12; }
void compile_d() { convert_h(); n=n*16+13; }
void compile_e() { convert_h(); n=n*16+14; }
void compile_f() { convert_h(); n=n*16+15; }
void compile_h() { convert_h(); compile_n(); }

struct e *compilednow=0;

static void do_ret() { *cc.b++=0xc3; }
int8_t *fwjumps[8], **fwjump;
static void do_if() {
	*cc.b++=0x75;
	*fwjump++=cc.c++;
}
static void compile_notif() { *cc.b++=0x74; *fwjump++=cc.c++; }
static void compile_ifns() { *cc.b++=0x79; *fwjump++=cc.c++; }

static void do_end() {
	--fwjump;
	**fwjump = cc.b - (uint8_t*)((*fwjump)+1);
}

int8_t *bwjumps[8], **bwjump;
static void compile_begin() {
	*bwjump++=cc.c;
}
static void compile_rewind() {
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
static void compile_nip() { *cc.b++=0x8d; *cc.b++=0x76; *cc.b++=0x04; }
static void compile_add() { *cc.b++=0x03; *cc.b++=0x06; compile_nip(); }
static void compile_sub() { *cc.b++=0x2b; *cc.b++=0x06; compile_nip(); }
static void compile_over() { compile_dup(); *cc.b++=0x8b; *cc.b++=0x46; *cc.b++=0x04; }
static void compile_swap() { *cc.b++=0x87; *cc.b++=0x06; }

void compile_call(void *a) { *cc.b++=0xe8; *cc.i++=((uint8_t*)a)-(cc.b+4); }

static void do_spot() {
	register uint32_t *stack asm("esi");
	uint32_t *s = (void *)(stack[0]);
	cairo_set_source_rgb(cr,0.8,0.8,0.8);
	cairo_arc(cr,s[0],s[1],2,0,7);
	cairo_fill(cr);
}

static void compile_spot() {
	compile_dup();
	compile_call(do_spot);
	compile_drop();
	compile_drop();
}

static void delay(struct tag *w) {
	*cc.i=0;
	dec->p=cc.i++; dec->w=w; dec++;
}

int execute_it=0;
struct tag *current;

static void do_allot() {
	register uint32_t *stack asm("esi");
	current->l = (stack[0]);
	printf("realloc %08x (%d)", (uint32_t)current->data, current->l);
	current->data=realloc(current->data,current->l);
	printf(" -> %08x (%d)\n", (uint32_t)current->data, current->l);
}

static void compile_allot() {
	execute_it=1;
	compile_dup();
	compile_call(do_allot);
	compile_drop();
	compile_drop();
}

uint8_t *beg;

inline void compilelist(struct tag *t) {
	printf("compile %s\n", t->s);
	fwjump=fwjumps;
	bwjump=bwjumps;
	execute_it=0;
	current = t;

	beg=cc.b;

	t->gen++;

	struct e *e=t->def;
	for(;e;e=e->n) {
		switch(e->t) {
		case builtin: { void (*f)(void)=e->o->data; f(); } break;
		case data:
			compile_dup();
			assert(e->o->gen==gen);
			compile_imm((uint32_t)e->o->data);
			break;
		case macro:
			assert(e->o->gen==gen);
			execute(e->o->data);
			break;
		default:
			*cc.b++=0xe8;
			if(e->o->gen!=gen) { delay(e->o); }
			else if(e->o==t) { *cc.i++=((uint8_t*)e->o->data)-(beg+4); }
			else { *cc.i++=((uint8_t*)e->o->data)-(cc.b+4); }
		}
	}
	if(execute_it) execute((void *)beg);
	else t->data=beg;
}

struct tag *plan[256];

static void do_plan() {
	struct tag *t;
	struct e *depth[10],**d;
	struct tag **p=plan;
	gen+=2;
	for(t=words.heads;t<words.end;t++) {
		if(t->gen==gen) continue;
		d=depth;
		struct e *e=t->def;
		t->gen++;
		printf("root: %s\n", t->s);
		for(;;) {
			for(;e;e=e->n) {
				if(e->o->t==builtin) continue;
				if(e->o->gen==gen) continue; // compiled
				if(e->o->gen==gen-1) {
					if(e->t==macro) { printf("circular 1 %s\n", e->o->s); return; }
					struct e **p=d-1;
					for(;p>=depth && (*p)->o!=e->o;p--) {
						if((*p)->t == macro) { printf("circular 2 %s\n", e->o->s); return; }
					}
					continue; // will backpatch it
				}
				break;
			}
			if(!e) {
				if(--d<depth) break;
				(*d)->o->gen++;
				printf(" < %d/%d %*s'%s'\n", (*d)->o->gen, gen, (d-depth)*3, "", (*d)->o->s);
				*p++=(*d)->o;
				e=(*d)->n;
			} else {
				printf(" > %d/%d %*s'%s'\n", e->o->gen, gen, (d-depth)*3, "", e->o->s);
				e->o->gen++;
				*d++=e;
				e=e->o->def;
			}
		}
		t->gen++;
		*p++=t;
	}
	*p=0;
}

static void do_compile() {
	cc.b=ccode;
	struct tag **t;
	do_plan();
	dec=decs;
	gen++;
	for(t=plan;*t;t++) { compilelist(*t); }
	while(--dec>=decs) {
		*dec->p=((uint8_t *)dec->w->data-(uint8_t*)(dec->p+1));
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
		e->t=t->t;
		e->n=*edit.pos;
		*edit.pos=e;
		edit.pos=&e->n;
	} else if(t->t!=builtin && t==selected) {
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

	if(edit.tag) { if(clickeditor(&edit,x1,y1)) return; }

	for(e=commands.heads;e<commands.end;e++) { if(clickcommand(e,x1,y1)) return; }

	for(e=words.heads;e<words.end;e++) { if(clicktag(e,x1,y1)) return; }
	for(e=builtins.heads;e<builtins.end;e++) { if(clicktag(e,x1,y1)) return; }

	if(edit.tag) { edit.x=x1 & 0xfffffff0; edit.y=y1 & 0xfffffff0; draw(); return; }
	if(selected) { selected->x=x1 & 0xfffffff0; selected->y=y1 & 0xfffffff0; draw(); return; }

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

void go() {
	if(words.heads->data) { execute(words.heads->data); }
}

