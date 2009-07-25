#include <stdio.h>

#include <cairo.h>
#include <cairo-xlib.h>

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

extern void init(cairo_t *);
extern void draw();
extern void button(int,int);
extern void release(int,int);
extern void key(int);
extern void go();
extern void motion(int,int);

static cairo_t *cr=0;
static cairo_surface_t *surface;

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
    Visual *visual = DefaultVisual(dpy, DefaultScreen (dpy));

    XClearWindow(dpy, win);

    surface = cairo_xlib_surface_create (dpy, win, visual,
					 w->width, w->height);
    init(cr=cairo_create(surface));
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

    win = XCreateSimpleWindow(dpy, root, 0, 0,
				   w->width, w->height, 0,
				   WhitePixel(dpy, w->scr), WhitePixel(dpy, w->scr));

    XSelectInput(dpy, win,
		 KeyPressMask|StructureNotifyMask|ExposureMask|ButtonPressMask|ButtonReleaseMask|Button1MotionMask);

    XMapWindow(dpy, win);
}

static void
win_deinit()
{
    cairo_destroy(cr);
    cairo_surface_destroy (surface);
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
		cairo_xlib_surface_set_size(surface,cev->width,cev->height);
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
