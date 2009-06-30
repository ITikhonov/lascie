#include <stdio.h>

#include "common.h"
#include "draw.h"

void addtoeditor(struct tag *t) {
	int i;
	for(i=0;i<10;i++) {
		if(editor.w[i]==0) {
			editor.w[i]=t;
			printf("add %s\n", t->s);
			draw();
			return;
		}
		
	}
}

