#include <stdio.h>

#include <string.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "common.h"
#include "lasca.h"
#include "draw.h"

static int clicktag(struct tag1 *t, int x1,int y1) {
	if(!(t->y<=y1 && y1<=t->y+t->w->h && x1>t->x && x1<t->x+t->w->w+(t->nospace?0:10))) return 0;
	if(t->t==command) { void (*f)(void)=(void *)t->w->data; f(); return 1; }
	draw();
	return 1;
}

void release(int x1,int y1) {
	struct tag1 *t;
	for(t=tags.tags;t<tags.end;t++) { if(clicktag(t,x1,y1)) return; }
	draw();
}

void button(int x1, int y1) {
}

void key(int k) {
	if(0) {
		switch(k) {
			case XK_BackSpace:
				draw();
				break;
			case XK_Delete:
			case XK_Return:
			case '0'...'9':
			case 'a'...'z': {
				}
		}
	}
}

