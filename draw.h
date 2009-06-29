#include <cairo.h>

#include "common.h"
#include "compiler.h"

struct editor { struct tag *tag; struct e **pos; int x, y; };
extern struct editor editor;
extern struct tag *selected;
extern int button_height;
void draw();
void resize(struct tag *c);

void openeditor(struct tag *t);
void drawinit(cairo_t *cr1);
void do_hexed();

void draw();

void shift(int *);
void unshift(int *);
