
CFLAGS=-Wall
PROGS=boot-extract

all: $(PROGS)

boot-extract: boot-extract.c

clean:
	rm -f *.o
	rm -f $(PROGS)

