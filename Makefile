all: raid5d

raid5d: raid5d.o
raid5d.o: raid5d.c

clean:
	rm -f raid5d raid5d.o

distclean: clean

.PHONY: all clean distclean
