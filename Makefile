fuseloop: fuseloop.c
	$(CC) $^ `pkg-config --cflags --libs fuse` -lpthread -o $@

.PHONY: clean
clean:
	-rm -f fuseloop
