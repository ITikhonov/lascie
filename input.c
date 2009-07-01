#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "common.h"
#include "lasca.h"
#include "draw.h"

static struct e *clicked;

static int clicktag(struct tag1 *t, int x1,int y1) {
	if(!(t->y<=y1 && y1<=t->y+t->e.w->h && x1>t->x)) return 0;
	int x=t->x+width(&t->e);
	if(x1<x) { clicked=&t->e; return 1; }

	struct e *e=t->e.w->def;
	for(;e;e=e->n) {
		x+=width(e);
		if(x1<x) return 1;
	}
	
	return 0;
}

enum mode { choose, move } mode;

static void clonetag(int x1, int y1) {
	struct tag1 *t=tags.end++;
	t->e=*clicked;
	t->x=x1;
	t->y=y1;
	clicked=&t->e;
	mode=move;
}

#define TAG(x) ((struct tag1 *)x)

void release(int x1,int y1) {
	if(!clicked) { selected=0; draw(); return; }
	switch(mode) {
		case move: TAG(clicked)->x=x1; TAG(clicked)->y=y1; draw(); return;
		case choose:
			if(clicked->t==command) { void (*f)(void)=(void *)clicked->w->data; f(); return; }
	}
	selected=clicked;
	draw();
}


static void opentag(int x1, int y1) {
	clonetag(x1,y1);
	TAG(clicked)->open=1;
}

static void deletetag() {
	tags.end--;
	*TAG(clicked)=*tags.end;
	clicked=0;
	draw();
}

void motion(int x1, int y1) {
	if(!clicked) return;

	int cy=TAG(clicked)->y;
	int cx=TAG(clicked)->x;
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
	} else if(mode==move) {
		TAG(clicked)->x=x1; TAG(clicked)->y=y1;
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

