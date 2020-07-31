# compiler flags
CFLAGS= -O2 -Wall

.PHONY: clean install stats

all: aldl-analyzer

aldl-analyzer: analyzer.c csv.o loadconfig.o config.h useful.o
	gcc $(CFLAGS) -o aldl-analyzer analyzer.c csv.o loadconfig.o useful.o

error: error.c error.h
	gcc $(CFLAGS) -c error.c

csv.o: csv.c csv.h config.h useful.h
	gcc $(CFLAGS) -c csv.c

loadconfig.o: loadconfig.c loadconfig.h config.h
	gcc $(CFLAGS) -c loadconfig.c

useful.o: useful.c useful.h
	gcc $(CFLAGS) -c useful.c

install: aldl-analyzer analyzer.conf
	cp -nv analyzer.conf /etc/aldl/analyzer.conf
	cp -v aldl-analyzer /usr/local/bin/aldl-analyzer

clean:
	rm -f aldl-analyzer *.o

stats:
	wc -l *.c *.h */*.c */*.h
