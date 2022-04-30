OUT=broff
SRC=broff.c
CFLAGS=-Og -g
#CFLAGS=-O2

ESCAPES_PROGRAM=broff-escapes

.PHONY: all clean test install uninstall

all: $(OUT)

run: $(OUT)
	./$(OUT)

test: $(OUT)
	./$(OUT) test.ms | ./$(ESCAPES_PROGRAM)

clean:
	rm -f $(OUT)

install: $(OUT)
	install $(OUT) /usr/local/bin/$(OUT)
	install $(ESCAPES_PROGRAM) /usr/local/bin/$(ESCAPES_PROGRAM)

uninstall: $(OUT)
	rm /usr/local/bin/$(OUT)
	rm /usr/local/bin/$(ESCAPES_PROGRAM)

$(OUT): $(SRC) Makefile
	gcc $(CFLAGS) $(SRC) -o $(OUT)
