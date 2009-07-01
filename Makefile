CFLAGS=-g -I/usr/include/cairo -Wall -Werror
LDFLAGS=-lcairo -lpthread -lasound

all: lasca

lasca: lasca.o main.o compiler.o draw.o input.o common.o

compiler.o: common.h lasca.h Makefile
draw.o: draw.c common.h compiler.h Makefile
lasca.o: lasca.c common.h compiler.h draw.h Makefile
main.o: main.c Makefile
input.o: input.c common.h lasca.h draw.h Makefile
common.o: common.h common.c
