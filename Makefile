CFLAGS=-g -Ih -I/usr/include/cairo -Wall -Werror
LDFLAGS=-lcairo -lpthread -lasound

all:
	cc $(CFLAGS) -c -o lasca.o lasca.c
	cc $(CFLAGS) -c -o main.o main.c
	cc $(CFLAGS) -c -o draw.o draw.c
	cc $(CFLAGS) -c -o input.o input.c
	cc $(CFLAGS) -c -o compiler.o compiler.c
	cc $(CFLAGS) -c -o common.o common.c
	cc $(LDFLAGS) lasca.o main.o compiler.o draw.o input.o common.o   -o lasca

