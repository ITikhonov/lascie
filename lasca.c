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

void draw();


void do_exit() { exit(0); }

#if 0
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
		p->draw=draw_button;
	}

	read(f,&n,4);
	while(n--) {
		struct nm *p=nme++;
		read(f,&p->s,8);
		read(f,&p->len,4);
		p->def.b=malloc(p->len);
		read(f,p->def.b,p->len);
		read(f,&p->ntype,4);
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
		write(f,&r->ntype,4);
	}
	close(f);
}

#endif

static uint8_t *ccode;
uint8_t *compile_at;

uint8_t *nests[5];
uint8_t **cnest;

void do_ret() {
	*compile_at++=0xc3;
}

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

#if 0

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
			switch(nmu[*i].ntype) {
			case macro: {
					void (*f)(void)=(void*)nmu[*i].def.b;
					compile_at=c; f(); c=compile_at;
				} break;
			case command:
			case compiled:
				*c=0xe8; c++;
				if(*i<0) { *((int32_t *)c)=(int32_t)(nmu[*i].def.b)-(int32_t)(c+4); }
				else { *((int32_t *)c)=(int32_t)(ccode+5*(*i))-(int32_t)(c+4); }
				c+=4;
				break;
			case data:
				abort(); // to be defined
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

#endif

void resize(struct c1 *c) {
	cairo_text_extents_t te;
	cairo_text_extents(cr,c->s,&te);
	c->w=te.x_advance+10; c->h=button_height+5;
}

struct c1 *add(int x, int y, char *s, void *f, int len, int t) {
	union c c;
	c.op=drawcode_e;

	c.c1->op=1; c.c1->x=x; c.c1->y=y; c.c1->t=t; c.c1->data=f; c.c1->l=len;
	strncpy(c.c1->s,s,7);
	resize(c.c1);
	struct c1 *r=c.c1++;
	drawcode_e=c.op;
	return r;
}

union c edit={0};

void do_create() { edit.c1=add(100, 100, "", 0, 0, compiled); draw(); }

int8_t hexext;

void do_ping(void) { puts("PONG"); }

void do_decrement(void) { (*stack)--; }

void alsa_init();

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

	alsa_init();

	add(170,30,";", do_ret,0,macro);
	add(170,30,"forever", do_true,0,macro);
	add(140,30,"<<", do_loop,0,macro);
	add(100,30,"{", do_question,0,macro);
	add(120,30,"}", do_unnest,0,macro);
	add(30,290,"ping", do_ping,0,command);
	ccode=(uint8_t *)malloc(65536);
	add(30,270,"code", ccode,65536,data);
#if 0
	add(30,250,"compile", do_compile,0,command);
	add(30,230,"load", do_load,0,command);
	add(30,210,"save", do_save,0,command);
	add(30,310,"run", do_run,0,command);
#endif
	add(30,50,"exit", do_exit,0,command);
	add(30,30,"create", do_create,0,command);

}

int dull=0;

void padcolor() { if(!dull) { cairo_set_source_rgb(cr,0.5,0.9,0.5); } else { cairo_set_source_rgb(cr,0.8,0.8,0.8); } }
void textcolor() { cairo_set_source_rgb(cr,0.0,0.0,0.0); }

union c c;
int x,y;
struct c1 *o;

void load() {
	switch(*c.op) {
	case 1: x=c.c1->x; y=c.c1->y; o=c.c1; return;
	case 2: o=(struct c1 *)(drawcode+c.c2->p); return;
	}
}

void first() { c.op=drawcode; load(); }
void next() {
	switch(*c.op) {
	case 1: c.c1++; break;
	case 2: c.c2++; break;
	}
	load();
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

	cairo_set_source_rgb(cr,0.1,0.1,0.1);

	first();
	while(*c.op!=0) {
		dull=edit.op?(c.op!=edit.op):0;
		label();
		next();
	}
}

void button(int x1,int y1) {
	first();
	while(*c.op!=0) {
		dull=edit.op?(c.op!=edit.op):0;
		if(x<=x1 && x1<=x+o->w  && y<=y1 && y1<=y+o->h) {
			switch(*c.op) {
			case 1: switch(o->t) {
					case command:
						{ void (*f)(void)=(void *)o->data; f(); } return;
					case compiled:
						if(edit.op) { o->open=0; } else {edit.c1=o; o->open=1; }
						draw(); return;
				}
			case 2: edit.op=c.op; return;
			}
		}
		next();
	}
	edit.op=0;
	draw();
}


void key(int k) {
	if(edit.op&&*edit.op==1) {
		switch(k) {
		case XK_BackSpace:
			edit.c1->s[strlen(edit.c1->s)]=0;
			resize(edit.c1);
			draw();
			break;
		case XK_Return:
			edit.op=0; draw(); break;
		default: {
			int l=strlen(edit.c1->s);
			edit.c1->s[l]=k;
			edit.c1->s[l+1]=0;
			resize(edit.c1);
			draw();
			}
		}
			
	}
}


#if 0
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

	add(140,120,"sin", alsa_sin);
	bte[-1].n->ntype=compiled;

	add(140,60,"sound", soundbuf);
	bte[-1].n->len=sizeof(soundbuf);
	bte[-1].n->ntype=data;
	bte[-1].pos=0;

	add(140,90,"sound#", &soundbufno);
	bte[-1].n->len=4;
	bte[-1].n->ntype=data;
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

#else
void alsa_init() {}
#endif

