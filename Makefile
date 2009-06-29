CFLAGS=-g -I/usr/include/cairo -Wall -Werror
LDFLAGS=-lcairo -lpthread -lasound

all: lasca

lasca: lasca.o main.o common.o compiler.o



