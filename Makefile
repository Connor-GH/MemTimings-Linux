
all: timings

timings: timings.c
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f timings
