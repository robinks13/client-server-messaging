CC ?= cc
CFLAGS ?= -Wall

COMMON = src/common.c
HEADERS = src/common.h src/msg_struct.h
RUNTIME_DIRS = history files downloads

.PHONY: all clean

all: client server $(RUNTIME_DIRS)

client: src/client.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ src/client.c $(COMMON)

server: src/server.c $(COMMON) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ src/server.c $(COMMON)

$(RUNTIME_DIRS):
	mkdir -p $(RUNTIME_DIRS)

clean:
	rm -f client server src/*.o
