#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "common.h"
#include "lasca.h"
#include "draw.h"

static int cx,cy,cscroll;
static struct e *prev, *sprev;
static struct e *clicked;
static struct tag1 *ctag;

enum mode { choose, move, noop, scroll } mode;

static int clicktag(struct tag1 *t, int x1,int y1) {
	if(!(t->y<=y1 && x1>t->x)) return 0;
	if(y1>t->y+t->e->w->h) {
		if(t->open && t->e->t == data) {
			if(x1<t->x+16*8+5 && y1<t->y+4*(button_height)+5) {
				cx=x1; cy=y1; ctag=t; mode=scroll; clicked=t->e; cscroll=t->scroll;
				return 1;
			}
		}
		return 0;
	}
	cx=t->x; cy=t->y; ctag=t;
	int x=cx;
	prev=0;
	struct e *e=t->e;
	for(;e;e=e->n) {
		cx=x;
		x+=width(e);
		if(x1<x) {
			clicked=e;
			return 1;
		}
		if(!t->open) return 0;
		prev=e;
	}
	
	return 0;
}


static void clonetag(int x1, int y1) {
	struct tag1 *t=tags.end++;
	t->e=&clicked->w->def;
	t->x=x1;
	t->y=y1;
	t->open=0;
	clicked=t->e;
	ctag=t;
	mode=move;
}

void release(int x1,int y1) {
	switch(mode) {
		case noop: break;
		case move: ctag->x=x1; ctag->y=y1; draw(); return;
		case choose:
			if(!clicked) { sprev=selected=0; }
			else if(clicked->t==command) { void (*f)(void)=(void *)clicked->w->data; f(); return; }
			else if(sprev&&selected) {
				struct e *e=editcode_e++;
				*e=*clicked;
				e->n=sprev->n;
				sprev->n=e;
				sprev=e;
				draw();
				return;
			} else {
				sprev=prev; selected=clicked;
			}
		case scroll:;
	}

	draw();
}


static void opentag(int x1, int y1) {
	clonetag(x1,y1);
	ctag->open=1;
	struct e *e=clicked;
	for(;e->n;e=e->n) { sprev=e; }
	selected=e;
}

static void deletetag() {
	if(prev) {
		if(clicked->n) {
			prev->n=prev->n->n;
			if(selected==clicked) { selected=clicked->n; sprev=prev; }
			else if(sprev==clicked) { sprev=prev; }
		}
	} else {
		tags.end--;
		*ctag=*tags.end;
		if(selected==clicked) { selected=0; }
	}
	clicked=0;
	mode=noop;
	draw();
}

void motion(int x1, int y1) {
	if(!clicked) return;
	int w=width(clicked);
	int h=clicked->w->h;

	if(mode==choose) {
		if(y1<cy) /* over */ {
			int dy=cy-y1;
			int lx=cx-dy;
			int rx=cx+w+dy;
			if(x1<lx) /* left */ {
				deletetag();
			} else if(x1>rx) /* right */ {
				opentag(x1,y1);
			} else /* top */ { 
				mode = move;
			}
		}
		else if(y1>cy+h) /* below */ {
			int dy=y1-(cy+h);
			int lx=cx-dy;
			int rx=cx+w+dy;
			if(x1<lx) /* left */ {
				deletetag();
			} else if(x1>rx) /* right */ {
				opentag(x1,y1);
			} else /* bottom */ {
				clonetag(x1,y1);
			}
			
		} else {
			int lx=cx;
			int rx=cx+w;
			if(x1<lx) /* left */ {
				deletetag();
			} else if(x1>rx) /* right */ {
				opentag(x1,y1);
			}
		}
	} else if(mode==scroll) {
		int n=cscroll + (y1-cy)/4 + 4*((x1-cx)/8);
		if(n<0) { ctag->scroll=0; }
		else if(n>ctag->e->w->len/8) {
			ctag->scroll=ctag->e->w->len/8;
		} else {
			ctag->scroll=n;
		}
		draw();
	} else if(mode==move) {
		ctag->x=x1; ctag->y=y1;
		draw();
	}
}

void button(int x1, int y1) {
	struct tag1 *t;
	mode=choose;
	for(t=tags.tags;t<tags.end;t++) { if(clicktag(t,x1,y1)) return; }
	clicked=0;
}

void key(int k) {
	if(selected) {
		switch(k) {
			case XK_BackSpace:
				selected->w->s[strlen(selected->w->s)-1]=0;
				resize(selected->w);
				draw();
				break;
			case XK_Delete:
			case XK_Return:
			case '0'...'9':
			case 'a'...'z': {
				char *s=selected->w->s;
				int l=strlen(s);
				s[l]=k;
				s[l+1]=0;
				resize(selected->w);
				draw();
				}
		}
	}
}

