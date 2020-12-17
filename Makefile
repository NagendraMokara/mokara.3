CC=gcc
CFLAGS=-g -Wall

all: oss user

oss: oss.c oss.h
	$(CC) $(CFLAGS) oss.c -o oss

user: user.c oss.h
	$(CC) $(CFLAGS) user.c -o user

clean:
	rm -f oss user
