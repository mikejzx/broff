OUT=broff
SRC=broff.c
#CFLAGS=-Og -g
CFLAGS=-O2

.PHONY: all clean test install uninstall

all: $(OUT)

run: $(OUT)
	./$(OUT)

test: $(OUT)
	./$(OUT) test.ms

clean:
	rm -f $(OUT)

install: $(OUT)
	install $(OUT) /usr/local/bin/$(OUT)

uninstall: $(OUT)
	rm /usr/local/bin/$(OUT)

$(OUT): $(SRC) Makefile
	gcc $(CFLAGS) $(SRC) -o $(OUT)
