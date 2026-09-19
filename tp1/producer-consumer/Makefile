CC     = gcc
CFLAGS = -Wall -O2 -std=c11

all: produtor_consumidor

produtor_consumidor: produtor_consumidor.c
	$(CC) $(CFLAGS) -o produtor_consumidor produtor_consumidor.c -lpthread

clean:
	rm -f produtor_consumidor produtor_consumidor.exe