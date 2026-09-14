CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -O2
LDFLAGS =
EXECUTABLES = metinfo metrepair

.PHONY: all clean install uninstall

all: $(EXECUTABLES)

metinfo: metinfo.o
	$(CC) $(LDFLAGS) -o $@ $^

metrepair: metrepair.o
	$(CC) $(LDFLAGS) -o $@ $^

metinfo.o: metinfo.c metfmt.h
	$(CC) $(CFLAGS) -c $<

metrepair.o: metrepair.c metfmt.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(EXECUTABLES) *.o

install: all
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(EXECUTABLES) $(DESTDIR)/usr/local/bin

uninstall:
	rm -f $(addprefix $(DESTDIR)/usr/local/bin/,$(EXECUTABLES))
