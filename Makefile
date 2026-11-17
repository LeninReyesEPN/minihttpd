CC = gcc

CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude

SRC	= $(wildcard src/*.c)
OBJ	= $(SRC:.c=.o)
PROG	= minihttpd

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROG) src/*.o

.PHONY: all clean
