CFLAGS=-g -I/usr/include/cairo -Wall -Werror
LDFLAGS=-lcairo

all: lasca

lasca: lasca.o main.o

