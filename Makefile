CFLAGS=-g -I/usr/include/cairo -Wall
LDFLAGS=-lcairo

all: lasca

lasca: lasca.o main.o

