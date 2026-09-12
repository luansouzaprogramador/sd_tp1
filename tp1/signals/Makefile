CC = gcc
CFLAGS = -Wall -O2

all: emissor receptor

emissor: emissor.c
	$(CC) $(CFLAGS) -o emissor emissor.c

receptor: receptor.c
	$(CC) $(CFLAGS) -o receptor receptor.c

clean:
	rm -f emissor receptor