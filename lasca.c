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

struct nm { char s[8]; union { uint8_t *b; int *i; void (*f)(void); } def; int len, data; } nms[100];
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


void do_down() { if(which) { which->pos=max(0,min(which->pos+8*8,(which->n->len/8)*8)); draw(); } }

void do_up() { if(which) { which->pos=max(0,which->pos-8*8); draw(); } }

static uint8_t *ccode;


uint8_t *compile_at;


uint8_t *nests[5];
uint8_t **cnest;

void do_true() {
	*compile_at++=0x31;
	*compile_at++=0xc0;
}

void do_question() {
	*compile_at++=0x75; // JNE Jump short if not equal (ZF=0)
	*cnest++=compile_at;
	*compile_at++=0x00; // placeholder for unnest
	
}

void do_loop() {
	*compile_at++=0xeb;
	int off=((uint32_t)(*(cnest-1)-4))-((uint32_t)compile_at);
	*compile_at++=(int8_t)off;
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

pthread_t runt;

void *_do_run(void *p) {
	void (*f)(void)=p;
	f();
	pthread_exit(0);
}

void do_run() {
	if(which) { pthread_create(&runt,0,_do_run,(void *)ccode+5*(which->n-nmu)); }
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

int8_t hexext;

void do_ping(void) { puts("PONG"); }

void do_decrement(void) { (*stack)--; }

void alsa_init();

void init(cairo_t *cr1) {
	alsa_init();
	add(30,310,"run", do_run);

	add(170,30,"forever", do_choose);
	bte[-1].n->def.f=do_true;
	bte[-1].n->data=2;
	bte[-1].n->len=0;

	add(140,30,"<<", do_choose);
	bte[-1].n->def.f=do_loop;
	bte[-1].n->data=2;
	bte[-1].n->len=0;

	add(100,30,"{", do_choose);
	bte[-1].n->def.f=do_question;
	bte[-1].n->data=2;
	bte[-1].n->len=0;

	add(120,30,"}", do_choose);
	bte[-1].n->def.f=do_unnest;
	bte[-1].n->data=2;
	bte[-1].n->len=0;

	add(30,290,"ping", do_ping);
	bte[-1].n->def.f=do_ping;
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

	int n;
	char s[2]={0,0};
	for(n='0';n<='9';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }
	for(n='a';n<='f';n++) { s[0]=n; cairo_text_extents(cr,s,&te); if(hexext<te.x_advance) hexext=te.x_advance; }
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
	int w=hexext*2*8+15;
	int h=button_height*(1+min(bt->n->len/8,8))+10;
	bt->x2=bt->x+w; bt->y2=bt->y+h;

	if(bt==which) { cairo_set_source_rgb(cr,0.9,0.5,0.5); } else { cairo_set_source_rgb(cr,0.5,0.9,0.5); }
	cairo_rectangle(cr,bt->x,bt->y,w,h);
	cairo_fill(cr);

	cairo_set_source_rgb(cr,0.1,0.1,0.1);

	int i=0,x=bt->x+5,y=bt->y+button_height;

	cairo_move_to (cr, x, y);
	cairo_show_text (cr, bt->n->s);

	uint8_t *p=bt->n->def.b+which->pos;
	char hex[16]="0123456789abcdef";
	char s[3]={0,0,0};

	for(;i<(bt->n->len-which->pos) && i<64;i++) {
		if(i%8==0) { y+=button_height; x=bt->x; }
		if(i%4==0) { x+=5; }
		s[0]=hex[(*p)>>4]; s[1]=hex[(*p)&0xf];
		cairo_move_to (cr, x, y);
		cairo_show_text(cr,s);
		x+=hexext*2; p++;
	}
	cairo_stroke(cr);
}

void draw() {
	cairo_save(cr);
	cairo_set_source_rgb(cr,255,255,255);
	cairo_paint(cr);

	cairo_set_source_rgb(cr,0.1,0.1,0.1);
	struct bt *p=bts; for(;p<bte;p++) { p->draw(p); }
}

void button(int x,int y) { if(ops->button) ops->button(x,y); }
void key(int k)		 { if(ops->key) ops->key(k); }


#if 1
snd_pcm_t *alsa_h;

int16_t soundbuf[2][2*441];
uint32_t soundbufno=0;

pthread_t sthread;

void *alsa_run(void *v) {
	for(;;) {
		int err=snd_pcm_writei(alsa_h,soundbuf[soundbufno&1],441);
                if(err<0) { snd_pcm_recover(alsa_h,err,0); }
                switch(snd_pcm_state(alsa_h)) {
                        case SND_PCM_STATE_RUNNING: break;
                        case SND_PCM_STATE_PREPARED: printf("start\n"); snd_pcm_start(alsa_h); break;
                        case SND_PCM_STATE_XRUN: printf("fucked"); break;
                        default: printf("wtf state %d\n", snd_pcm_state(alsa_h));
                }
		soundbufno++;
	}
}

void alsa_sin() { *stack=sin((2*M_PI/2048) * *stack); }


void alsa_init() {
	int err;

	add(140,120,"sin", do_choose);
	bte[-1].n->def.f=alsa_sin;
	bte[-1].n->data=0;

	add(140,60,"sound", do_choose);
	bte[-1].n->def.b=(uint8_t *)soundbuf;
	bte[-1].n->len=sizeof(soundbuf);
	bte[-1].n->data=1;
	bte[-1].pos=0;

	add(140,90,"sound#", do_choose);
	bte[-1].n->def.b=(uint8_t *)(&soundbufno);
	bte[-1].n->len=4;
	bte[-1].n->data=1;
	bte[-1].pos=0;

	int i; for(i=0;i<441*2;i+=2) { soundbuf[0][i]=soundbuf[0][i+1]=soundbuf[1][i]=soundbuf[1][i+1]=i; }

        if ((err = snd_pcm_open(&alsa_h, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
                printf("Playback open error: %s\n", snd_strerror(err));
                exit(1);
        }

        if ((err = snd_pcm_set_params(alsa_h,
                                     SND_PCM_FORMAT_S16,
                                     SND_PCM_ACCESS_RW_INTERLEAVED,
                                     2,
                                     44100,
                                     1,
                                     1000900)) < 0) {
               printf("Playback open error: %s\n", snd_strerror(err));
               exit(EXIT_FAILURE);
       }

       pthread_create(&sthread,0,alsa_run,0);
}


#endif

