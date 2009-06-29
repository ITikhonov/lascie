#include <stdio.h>

#include <cairo.h>
#include <cairo-xlib.h>

#define PI 3.1415926535

typedef struct win {
    Display *dpy;
    int scr;

    Window win;
    GC gc;

    int width, height;
} win_t;

static void win_init(win_t *win);
static void win_init2(win_t *win);
static void win_deinit(win_t *win);
static void win_handle_events(win_t *win);

extern void init(cairo_t *);
extern void draw();
extern void button(int,int);
extern void release(int,int);
extern void key(int);
extern void go();

static cairo_t *cr=0;
static cairo_surface_t *surface;

int
main(int argc, char *argv[])
{
    win_t win;

    win.dpy = XOpenDisplay(0);

    if (win.dpy == NULL) {
	fprintf(stderr, "Failed to open display\n");
	return 1;
    }

    win_init(&win);
    win_init2(&win);

    draw();

    win_handle_events(&win);

    win_deinit(&win);

    XCloseDisplay(win.dpy);

    return 0;
}


static void
win_init2(win_t *win)
{
    Visual *visual = DefaultVisual(win->dpy, DefaultScreen (win->dpy));

    XClearWindow(win->dpy, win->win);

    surface = cairo_xlib_surface_create (win->dpy, win->win, visual,
					 win->width, win->height);
    init(cr=cairo_create(surface));
}

static void
win_init(win_t *win)
{
    Window root;

    win->width = 400;
    win->height = 400;

    root = DefaultRootWindow(win->dpy);
    win->scr = DefaultScreen(win->dpy);

    win->win = XCreateSimpleWindow(win->dpy, root, 0, 0,
				   win->width, win->height, 0,
				   BlackPixel(win->dpy, win->scr), BlackPixel(win->dpy, win->scr));

    XSelectInput(win->dpy, win->win,
		 KeyPressMask|StructureNotifyMask|ExposureMask|ButtonPressMask|ButtonReleaseMask);

    XMapWindow(win->dpy, win->win);
}

static void
win_deinit(win_t *win)
{
    cairo_destroy(cr);
    cairo_surface_destroy (surface);
    XDestroyWindow(win->dpy, win->win);
}

static void
win_handle_events(win_t *win)
{
    XEvent xev;

    while (1) {
	while(XPending(win->dpy)==0) { go(); }
	XNextEvent(win->dpy, &xev);
	switch(xev.type) {
	case KeyPress:
	{
	    XKeyEvent *kev = &xev.xkey;
	    
	    key(XKeycodeToKeysym(win->dpy,kev->keycode,0));
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
	case Expose:
	{
	    XExposeEvent *eev = &xev.xexpose;
	    if (eev->count == 0) draw();
	}
	break;
	}
    }
}
