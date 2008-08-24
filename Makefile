# $Id: Makefile 8 2007-11-19 17:01:15Z leucos $

CC=gcc
CFLAGS=-g
LDFLAGS=-lcwiimote -lbluetooth -lm -llo
EXEC=wii2osc
SRC= $(wildcard *.c)
OBJ= $(SRC:.c=.o)

all: $(EXEC)

wii2osc.o:

wii2osc: $(OBJ)
	@echo compiling $(EXEC)
	@$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c debug.h
	@echo "Compiling $< to $@"
	@$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
	@rm -rf *.o *~

mrproper: clean
	@rm -rf $(EXEC)