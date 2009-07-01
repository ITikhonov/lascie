#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "common.h"
#include "lasca.h"
#include "draw.h"

static struct tag1 *clicked;

static int width(struct tag1 *t) {
	return t->w->w+(t->nospace?0:10);
}

static int clicktag(struct tag1 *t, int x1,int y1) {
	if(!(t->y<=y1 && y1<=t->y+t->w->h && x1>t->x && x1<t->x+width(t))) return 0;
	clicked=t;
	return 1;
}

enum mode { choose, move } mode;

static void clonetag(int x1, int y1) {
	struct tag1 *t=tags.end++;
	t->t=clicked->t;
	t->nospace=clicked->nospace;
	t->w=clicked->w;
	t->x=x1;
	t->y=y1;
	clicked=t;
	mode=move;
}

void release(int x1,int y1) {
	if(!clicked) { selected=0; draw(); return; }
	switch(mode) {
		case move: clicked->x=x1; clicked->y=y1; draw(); return;
		case choose:
			if(clicked->t==command) { void (*f)(void)=(void *)clicked->w->data; f(); return; }
	}
	selected=clicked;
	draw();
}


static void opentag(int x1, int y1) {
	clonetag(x1,y1);
	clicked->open=1;
}

static void deletetag() {
	tags.end--;
	*clicked=*tags.end;
	clicked=0;
	draw();
}

void motion(int x1, int y1) {
	if(!clicked) return;
	if(mode==choose) {
		if(y1<clicked->y) /* over */ {
			int dy=clicked->y-y1;
			int lx=clicked->x-dy;
			int rx=clicked->x+width(clicked)+dy;
			printf("over : %d %d %d\n", lx,x1,rx);
			if(x1<lx) /* left */ {
				deletetag();
			} else if(x1>rx) /* right */ {
				opentag(x1,y1);
			} else /* top */ { 
				mode = move;
			}
		}
		else if(y1>clicked->y+clicked->w->h) /* below */ {
			int dy=y1-(clicked->y+clicked->w->h);
			int lx=clicked->x-dy;
			int rx=clicked->x+width(clicked)+dy;
			printf("below: %d %d %d\n", lx,x1,rx);
			if(x1<lx) /* left */ {
				deletetag();
			} else if(x1>rx) /* right */ {
				opentag(x1,y1);
			} else /* bottom */ {
				clonetag(x1,y1);
			}
			
		} else {
			int lx=clicked->x;
			int rx=clicked->x+width(clicked);
			printf("side : %d %d %d\n", lx,x1,rx);
			if(x1<lx) /* left */ {
				deletetag();
			} else if(x1>rx) /* right */ {
				opentag(x1,y1);
			}
		}
	} else if(mode==move) {
		clicked->x=x1; clicked->y=y1;
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

