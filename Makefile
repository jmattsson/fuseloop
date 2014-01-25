CFLAGS:=$(shell pkg-config --cflags fuse) $(CFLAGS)
LDFLAGS:=$(shell pkg-config --libs fuse) $(LDFLAGS)

.PHONY: all
all: fuseloop

fuseloop: fuseloop.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

.PHONY: clean
clean:
	-rm -f fuseloop
