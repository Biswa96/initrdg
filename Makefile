# This file is part of initrg project.
# Licensed under the terms of the GNU General Public License v3 or later.

# Makefile: to compile initrd.img file

BIN = init
BINIMG = initrd.img
CFLAGS = -D_GNU_SOURCE -pedantic -O2 -std=c99 -Wall
LDFLAGS = -static -static-libgcc

all : $(BINIMG)

$(BIN) : fs.c main.c msg.c net.c proc.c util.c
	$(CC) -s $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BINIMG) : $(BIN)
	ls $^ | cpio -o -H newc -F $@

clean :
	rm -f $(BIN) $(BINIMG)
