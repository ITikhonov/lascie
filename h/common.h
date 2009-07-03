#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>

enum wordtype { compiled=0,special=1,builtin=2 };
enum tagtype { normal=0,data=1,macro=2,command=3 };

struct e { struct e *n; enum tagtype t; uint8_t nospace; struct word *w; };
struct word { uint32_t w,h; char s[8]; enum wordtype t; void *data; uint32_t len; struct e def; uint8_t gen; };
struct tag1 { struct e *e; uint32_t x,y; uint8_t open; };

struct voc { struct word w[256], *end; };
struct tags { struct tag1 tags[256], *end; };

extern struct e *selected;
extern int button_height;

extern int nospace;
extern struct e editcode[];
extern struct e *editcode_e;

extern struct voc words;
extern struct tags tags;

extern uint8_t gen;
extern uint32_t stackh[], *stack;

struct word *newword();

#endif
