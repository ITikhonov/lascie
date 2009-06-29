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
	if(editor.tag) {
		struct e *e=editcode_e++;
		e->o=t;
		e->t=t->t;
		e->n=*editor.pos;
		*editor.pos=e;
		editor.pos=&e->n;
	} else if(t->t!=builtin && t==selected) {
		openeditor(t);
	} else {
		selected=t;
	}
	draw();
	return 1;
}

static int x, y;

static inline int clickeditor(struct editor *ed, int x1, int y1) {
	if(!(y1>=100 && y1<=100+button_height+5 && x1>ed->x)) return 0;

	x=ed->x+ed->tag->w; y=ed->y;
	if(x1<=x) { ed->tag=0; draw(); return 1; }

	struct e **p=&ed->tag->def;
	for(;*p;p=&(*p)->n) {
		x+=(*p)->o->w;
		if(x1<=x) { ed->pos=p; draw(); return 1; }
	}
	return 0;
}

void release(int x1,int y1) {
	struct tag *e;
	if(editor.tag) { if(clickeditor(&editor,x1,y1)) return; }

	unshift(&y1);
	for(e=commands.heads;e<commands.end;e++) { if(clickcommand(e,x1,y1)) return; }

	for(e=words.heads;e<words.end;e++) { if(clicktag(e,x1,y1)) return; }
	for(e=builtins.heads;e<builtins.end;e++) { if(clicktag(e,x1,y1)) return; }

	if(editor.tag) { editor.tag=0; }
	else if(selected) { selected->x=x1 & 0xfffffff0; selected->y=y1 & 0xfffffff0; draw(); return; }

	draw();
}

void button(int x1, int y1) {
}

void key(int k) {
	if(editor.tag) {
		switch(k) {
			case XK_BackSpace:
				editor.tag->s[strlen(editor.tag->s)-1]=0;
				resize(editor.tag);
				draw();
				break;
			case XK_Delete:
				if((*editor.pos)->n) { *(editor.pos)=(*editor.pos)->n; draw(); } break;
			case XK_Return:
				editor.tag=0; draw(); break;
			case '0'...'9':
			case 'a'...'z': {
				char *s=editor.tag->s;
				int l=strlen(s);
				s[l]=k;
				s[l+1]=0;
				resize(editor.tag);
				draw();
				}
		}
	}
}

