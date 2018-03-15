#Compiler
CC=gcc
#Flags
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic

all: h2o

h2o: h2o.c
	$(CC) $(CFLAGS) h2o.c -o h2o -pthread

clean:
	rm h2o
