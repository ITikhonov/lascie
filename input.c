#include <string.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "common.h"
#include "lasca.h"
#include "draw.h"

static inline int clickcommand(struct tag *e, int x1,int y1) {
	if(!(e->y<=y1 && y1<=e->y+e->h && x1>e->x && x1<e->x+e->w)) return 0;
	void (*f)(void)=(void *)e->data; f(); return 1;
}


static inline int clicktag(struct tag *t, int x1,int y1) {
	if(!(t->y<=y1 && y1<=t->y+t->h && x1>t->x && x1<t->x+t->w)) return 0;
	draw();
	return 1;
}

void release(int x1,int y1) {
	struct tag *e;
	for(e=commands.heads;e<commands.end;e++) { if(clickcommand(e,x1,y1)) return; }
	for(e=words.heads;e<words.end;e++) { if(clicktag(e,x1,y1)) return; }
	for(e=builtins.heads;e<builtins.end;e++) { if(clicktag(e,x1,y1)) return; }
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

