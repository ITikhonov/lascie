#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>

enum nmflag { compiled=0, data=1, macro=2, command=3, builtin=4 };
struct tag { uint32_t x,y,w,h; char s[8]; uint8_t t; void *data; uint32_t len; uint8_t nospace; struct e *def; uint8_t gen; };
struct e { struct e *n; enum nmflag t; struct tag *o; };
struct voc { struct tag heads[256], *end; };

extern struct editor editor;
extern struct tag *selected;
extern int button_height;

extern int nospace;
extern struct e *editcode_e;

extern struct voc words;
extern struct voc commands;
extern struct voc builtins;

extern uint8_t gen;
extern uint32_t stackh[], *stack;

#endif
