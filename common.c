#include "common.h"

#undef NDEBUG
#include <assert.h>

int button_height=0;

struct e *editcode_e;

struct voc words = {.end=words.w};
struct tags tags = {.end=tags.tags};
struct e editcode[1024];


uint8_t gen=0;
uint32_t stackh[32]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}, *stack=stackh+30;

struct e *selected;

struct word *newword() { assert(words.end<(words.w+256)); return words.end++; }

