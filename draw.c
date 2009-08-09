#include <string.h>
#include <stdio.h>
#include <cairo.h>

#include "o/font.h"

#include "draw.h"

#include "common.h"
#include "compiler.h"
#include "lasca.h"

#include <cairo-xlib.h>

static cairo_t *cr=0;

extern Display *dpy;
extern GC gc;
extern Window win;

static void grab();

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


void drawinit(cairo_t *cr1) {
	cr=cr1;
	cairo_select_font_face (cr, "times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_set_line_width (cr,1);

	struct chare *e = chars+(unsigned char)'t';
	button_height= e->h+e->t;
}

char color[3];
static void normalcolor()  {color[0]=0x7f; color[1]=0xdf; color[2]=0x7f; cairo_set_source_rgb(cr,0.5,0.9,0.5);}
static void macrocolor()   {color[0]=0x7f; color[1]=0x7f; color[2]=0xdf; cairo_set_source_rgb(cr,0.5,0.5,0.9);}
static void datacolor()    {color[0]=0xdf; color[1]=0xdf; color[2]=0x7f; cairo_set_source_rgb(cr,0.9,0.9,0.5);}
static void commandcolor() {color[0]=0xdf; color[1]=0x7f; color[2]=0x7f; cairo_set_source_rgb(cr,0.9,0.5,0.5);}

static inline void dullcolor()   { cairo_set_source_rgb(cr,0.8,0.8,0.8); }

static inline void textcolor() { cairo_set_source_rgb(cr,0,0,0); }

static int x,y;

XImage *im;
unsigned int ww,wh;

static void grab() {
	unsigned int du;
	int ds;
	Window w;
	XGetGeometry(dpy,win,&w,&ds,&ds,&ww,&wh,&du,&du);
	im=XGetImage(dpy,win,0,0,ww,wh,AllPlanes,ZPixmap);
}

static void put() {
	XPutImage(dpy,win,DefaultGC(dpy,DefaultScreen(dpy)),im,0,0,0,0,ww,wh);
	XFree(im);
}

static void pad(struct e *e) {
	int xi,yi,wi=e->w->w+(e->nospace?0:10),hi=e->w->h;
	if((y+hi)>wh) hi=wh-y;
	if((x+wi)>ww) wi=ww-x;
	for(yi=0;yi<hi;yi++) {
		char *p=im->data+ (x*4) + ((y+yi)*4*im->width);
		for(xi=0;xi<wi;xi++) {
			*p++=color[0]; *p++=color[1]; *p++=color[2]; p++;
		}
	}
}

static int drawchar(int x0,int y0, char b) {
	int x;
	struct chare *c=chars+(unsigned char)b;
	unsigned char *r=(unsigned char*)(im->data+(y0-c->t)*4*ww+(x0+c->l)*4);
	unsigned char *re=(unsigned char*)(im->data+wh*ww*4);
	unsigned char *d=c->p;

	int size=c->w*c->h;
	int pitch=(ww-c->w)*4;

	for(x=0;x<size;x++) {
		if(r<(unsigned char *)(im->data)) continue;
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

static void drawtext(int x,int y,char *s) {
	for(;*s;s++) {
		x+=drawchar(x,y,*s);
	}
}

static void text(struct e *e) {
	drawtext(x+(e->nospace?0:5), y+button_height, e->w->s);
}

static void drawstack() {
	char s[20];
	cairo_move_to(cr, 5, 5+button_height);
	uint32_t *p=stack;

	textcolor();

	sprintf(s,"w: %d/%d ",words.end-words.w,sizeof(words.w)/sizeof(*words.w));
	cairo_show_text(cr, s);
	sprintf(s,"t: %d/%d ",tags.end-tags.tags,sizeof(tags.tags)/sizeof(*tags.tags));
	cairo_show_text(cr, s);
	sprintf(s,"e: %d/%d | ",editcode_e-editcode,sizeof(editcode)/sizeof(*editcode));
	cairo_show_text(cr, s);

	while(p<stackh+32) {
		sprintf(s,"%x ",*p);
		cairo_show_text(cr, s);
		p++;
	}
	cairo_stroke(cr);
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
	uint8_t *p = t->e->w->data;
	if(!p) return;
	int l = t->e->w->len;
	uint8_t *end=p+l;

	p+=t->scroll*8;
	int r,i;
	char s[4];
	x=t->x; y=t->y+button_height+5;

	{
		int w=16*8+5,h=4*(button_height)+5;

		typecolor(data);
		cairo_rectangle(cr,x,y,w,h);
		cairo_fill(cr);

		typecolor(command);
		int k=(h*t->scroll)/(t->e->w->len/8);
		cairo_rectangle(cr,x+w,y+k,2,2);
		cairo_fill(cr);

	}

	textcolor();
	for(r=0;r<4;r++) {
		x=t->x+5; y+=button_height;
		for(i=0;i<8;i++) {
			if(p>=end) goto done;
			cairo_move_to(cr,x,y);
			sprintf(s,"%02x",*p++);
			cairo_show_text(cr, s);
			x+=16;
		}
	}
done:
	cairo_stroke(cr);
}

static void drawlist(struct e *e, int isopen) {
	for(;e;e=e->n) {
		typecolor(e->t); pad(e);
		textcolor(); text(e);
		if(selected==e) { commandcolor(); cairo_rectangle(cr,x,y,width(e),e->w->h); cairo_stroke(cr); }
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
	drawstack();
	grab();
        cairo_set_source_rgb(cr,1,1,1);
        cairo_paint(cr);

	struct tag1 *t;
	for(t=tags.tags;t<tags.end;t++) { drawtag(t); }


	if(0) drawchar(10,10,'T');
	if(1) drawtext(10,10,"abcdcp.-;");
	put();
}

