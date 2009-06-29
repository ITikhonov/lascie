#include <cairo.h>

#include "common.h"
#include "compiler.h"

void draw();
void resize(struct tag *c);

void openeditor(struct tag *t);
void drawinit(cairo_t *cr1);
void do_hexed();

void draw();

void shift(int *);
void unshift(int *);
