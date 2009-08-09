#include <stdio.h>
#include <stdlib.h>

#include <GL/glx.h>

#define PI 3.1415926535

typedef struct win {
    int scr;

    int width, height;
} win_t;

Display *dpy;
GC gc;
Window win;

static void win_init(win_t *win);
static void win_init2(win_t *win);
static void win_deinit();
static void win_handle_events(win_t *win);

extern void init();
extern void draw();
extern void button(int,int);
extern void release(int,int);
extern void key(int);
extern void go();
extern void motion(int,int);

int windoww;
int windowh;
void *backbuffer;

int
main(int argc, char *argv[])
{
    win_t win;

    dpy = XOpenDisplay(0);

    if (dpy == NULL) {
	fprintf(stderr, "Failed to open display\n");
	return 1;
    }

    win_init(&win);
    win_init2(&win);

    XSync(dpy,False);
    draw();


    win_handle_events(&win);

    win_deinit(&win);

    XCloseDisplay(dpy);

    return 0;
}


static void
win_init2(win_t *w)
{
    XClearWindow(dpy, win);
    init();
}

GLXContext ctx;

void rebackbuffer(win_t *w) {
    backbuffer = malloc(w->width*w->height*4);
    windoww=w->width; windowh=w->height;
    glViewport(0,0,windoww,windowh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,windoww,windowh,0,0,1);
    glMatrixMode(GL_MODELVIEW);
}

static void
win_init(win_t *w)
{
    Window root;

    w->width = 400;
    w->height = 400;

    root = DefaultRootWindow(dpy);
    w->scr = DefaultScreen(dpy);
    gc = DefaultGC(dpy,w->scr);

    int attrib[]={GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None}; 
    XVisualInfo *visinfo = glXChooseVisual( dpy, w->scr, attrib );

    if(!visinfo) { abort(); }

    XSetWindowAttributes attr = { .colormap=XCreateColormap(dpy,root,visinfo->visual,AllocNone) };
    win = XCreateWindow(dpy, root, 0, 0, w->width, w->height, 0, visinfo->depth, InputOutput, visinfo->visual, CWColormap, &attr);


    XSelectInput(dpy, win,
		 KeyPressMask|StructureNotifyMask|ExposureMask|ButtonPressMask|ButtonReleaseMask|Button1MotionMask);

    ctx = glXCreateContext( dpy, visinfo, NULL, True );
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, ctx);
    rebackbuffer(w);
}

static void
win_deinit()
{
    XDestroyWindow(dpy, win);
}

static void
win_handle_events(win_t *win)
{
    XEvent xev;

    while (1) {
	while(XPending(dpy)==0) { go(); }
	XNextEvent(dpy, &xev);
	switch(xev.type) {
	case KeyPress:
	{
	    XKeyEvent *kev = &xev.xkey;
	    
	    key(XKeycodeToKeysym(dpy,kev->keycode,0));
	}
	break;
	case ConfigureNotify:
	{
	    XConfigureEvent *cev = &xev.xconfigure;
		win->width = cev->width;
		win->height = cev->height;
		rebackbuffer(win);
	}
	break;
	case ButtonPress:
	{
		XButtonEvent *b=&xev.xbutton;
		button(b->x,b->y);
	}
	break;
	case ButtonRelease:
	{
		XButtonEvent *b=&xev.xbutton;
		release(b->x,b->y);
	}
	break;
	case MotionNotify:
	{
		XMotionEvent *b=&xev.xmotion;
		motion(b->x,b->y);
	}
	break;
	case Expose:
	{
	    XExposeEvent *eev = &xev.xexpose;
	    if (eev->count == 0) draw();
	}
	break;
	}
    }
}
