fuseloop: fuseloop.c
	$(CC) `pkg-config --cflags --libs fuse` -lpthread $^ -o $@

.PHONY: clean
	-rm -f fuseloop
