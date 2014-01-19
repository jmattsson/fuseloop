CFLAGS:=$(shell pkg-config --cflags fuse) $(CFLAGS)
LDFLAGS:=$(shell pkg-config --libs fuse) $(LDFLAGS)

.PHONY: all
all: fuseloop

.PHONY: clean
clean:
	-rm -f fuseloop
