
all: timings

timings:
	$(CC) -o $@ $@.c $(CFLAGS)

clean:
	rm -f timings
