#include <string.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "o/font.h"

#include "draw.h"

#include "common.h"
#include "compiler.h"
#include "lasca.h"

extern Display *dpy;
extern GC gc;
extern Window win;
extern void *backbuffer;
extern int windoww, windowh;

void draw();

void resize(struct word *w) {
	int x=0;
	char *s=w->s;
	for(;*s;s++) {
		struct chare *c=chars+(unsigned char)*s;
		x+=c->a;
	}
	w->w=x; w->h=button_height+5;
}

int width(struct e *e) { return e->w->w+(e->nospace?0:10); }


void drawinit() {
	struct chare *e = chars+(unsigned char)'t';
	button_height= e->h+e->t;
}

char color[3];
static void normalcolor()  {color[0]=0x7f; color[1]=0xdf; color[2]=0x7f; }
static void macrocolor()   {color[0]=0x7f; color[1]=0x7f; color[2]=0xdf; }
static void datacolor()    {color[0]=0xdf; color[1]=0xdf; color[2]=0x7f; }
static void commandcolor() {color[0]=0xdf; color[1]=0x7f; color[2]=0x7f; }

static void textcolor() { }

static int x,y;

#define MIN(x,y) ((x)>(y) ? (y) : (x))

static void rect(int x0,int y,int w,int h) {
	int xm,ym;
	int x;

	ym=MIN(y+h,windowh);
	xm=MIN(x0+w,windoww);
	
	for(;y<ym;y++) {
		char *p=backbuffer + (x0*4) + (y*4*windoww);
		for(x=x0;x<xm;x++) {
			*p++=color[0]; *p++=color[1]; *p++=color[2]; *p++=0x0;
		}
	}
}

static void pad(struct e *e) {
	int xi,yi,wi=e->w->w+(e->nospace?0:10),hi=e->w->h;
	if((y+hi)>windowh) hi=windowh-y;
	if((x+wi)>windoww) wi=windoww-x;
	for(yi=0;yi<hi;yi++) {
		char *p=backbuffer + (x*4) + ((y+yi)*4*windoww);
		for(xi=0;xi<wi;xi++) {
			*p++=color[0]; *p++=color[1]; *p++=color[2]; p++;
		}
	}
}

static int drawchar(int x0,int y0, char b) {
	int x;
	struct chare *c=chars+(unsigned char)b;
	unsigned char *r=(unsigned char*)(backbuffer+(y0-c->t)*4*windoww+(x0+c->l)*4);
	unsigned char *re=(unsigned char*)(backbuffer+windowh*windoww*4);
	unsigned char *d=c->p;

	int size=c->w*c->h;
	int pitch=(windoww-c->w)*4;

	for(x=0;x<size;x++) {
		if(r<(unsigned char *)(backbuffer)) continue;
		if(r>re) break;
		int cc=0xff-*d++;
		*r=((int)*r*cc)/0xff; r++;
		*r=((int)*r*cc)/0xff; r++;
		*r=((int)*r*cc)/0xff; r++;
		*r++=0;
		if(x%c->w==(c->w-1)) r+=pitch;
	}
	return c->a;
}

static int drawtext(int x,int y,char *s) {
	for(;*s;s++) {
		x+=drawchar(x,y,*s);
	}
	return x;
}

static void text(struct e *e) {
	drawtext(x+(e->nospace?0:5), y+button_height, e->w->s);
}

static void drawstack() {
	char s[20];
	x=5; y=5+button_height;
	uint32_t *p=stack;

	sprintf(s,"w: %d/%d ",words.end-words.w,sizeof(words.w)/sizeof(*words.w));
	x=drawtext(x,y,s);
	sprintf(s,"t: %d/%d ",tags.end-tags.tags,sizeof(tags.tags)/sizeof(*tags.tags));
	x=drawtext(x,y,s);
	sprintf(s,"e: %d/%d | ",editcode_e-editcode,sizeof(editcode)/sizeof(*editcode));
	x=drawtext(x,y,s);

	while(p<stackh+32) {
		sprintf(s,"%x ",*p);
		x=drawtext(x,y,s);
		p++;
	}
}

static void typecolor(enum tagtype t) {
	switch(t) {
		case command: commandcolor(); break;
		case normal: normalcolor(); break;
		case macro: macrocolor(); break;
		case data: datacolor(); break;
	}
}

static void drawhex(struct tag1 *t) {
	x=t->x; y=t->y+button_height+5;
	{
		int w=16*8+5,h=4*(button_height)+5;

		typecolor(data);
		rect(x,y,w,h);

		if(t->e->w->len) {
			typecolor(command);
			int k=(h*t->scroll)/(t->e->w->len/8);
			rect(x+w,y+k,2,2);
		}
	}

	uint8_t *p = t->e->w->data;
	if(!p) return;
	int l = t->e->w->len;
	uint8_t *end=p+l;

	p+=t->scroll*8;
	int r,i;
	char s[4];

	textcolor();
	for(r=0;r<4;r++) {
		x=t->x+5; y+=button_height;
		for(i=0;i<8;i++) {
			if(p>=end) goto done;
			sprintf(s,"%02x",*p++);
			drawtext(x,y,s);
			x+=16;
		}
	}
done:;
}

static void drawlist(struct e *e, int isopen) {
	for(;e;e=e->n) {
		typecolor(e->t); pad(e);
		if(selected==e) {
			commandcolor(); rect(x,y,width(e),e->w->h);
			typecolor(e->t); rect(x+1,y+1,width(e)-2,e->w->h-2);
		}
		textcolor(); text(e);
		if(!isopen) return;
		x+=width(e);
	}
}

static void drawtag(struct tag1 *t) {
	x=t->x; y=t->y;
	struct e *e=t->e;
	drawlist(e,t->open);
	if(t->open && t->e->t == data) drawhex(t);
}

void draw() {
	memset(backbuffer,0xff,windoww*windowh*4);

	struct tag1 *t;
	for(t=tags.tags;t<tags.end;t++) { drawtag(t); }
	drawstack();

	glRasterPos2i(0,0);
	glPixelZoom(1.0, -1.0);
	glDrawPixels(windoww,windowh,GL_RGBA,GL_UNSIGNED_BYTE,backbuffer);
	glXSwapBuffers(dpy,win);
}

