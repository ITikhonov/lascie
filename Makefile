CFLAGS=-g -Ih -I/usr/include/cairo -Wall -Werror
LDFLAGS=-lcairo -lpthread -lasound

all:
	cc $(CFLAGS) -c -o o/lasca.o lasca.c
	cc $(CFLAGS) -c -o o/main.o main.c
	cc $(CFLAGS) -c -o o/draw.o draw.c
	cc $(CFLAGS) -c -o o/input.o input.c
	cc $(CFLAGS) -c -o o/compiler.o compiler.c
	cc $(CFLAGS) -c -o o/common.o common.c
	cc $(LDFLAGS) o/lasca.o o/main.o o/compiler.o o/draw.o o/input.o o/common.o   -o lasca

