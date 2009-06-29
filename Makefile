CFLAGS=-g -I/usr/include/cairo -Wall -Werror
LDFLAGS=-lcairo -lpthread -lasound

all: lasca

lasca: lasca.o main.o compiler.o draw.o input.o

compiler.o: common.h lasca.h
draw.o: draw.c common.h compiler.h
lasca.o: lasca.c common.h compiler.h draw.h
main.o: main.c
input.o: input.c common.h lasca.h draw.h
