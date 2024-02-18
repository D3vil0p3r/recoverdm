VERSION=0.20

CFLAGS=-Wall -Wshadow -Wconversion -Wwrite-strings -Winline -O2 -DVERSION=\"$(VERSION)\"
LDFLAGS=

OBJSr=recoverdm.o dev.o io.o utils.o error.o
OBJSm=mergebad.o io.o utils.o error.o

all: recoverdm mergebad

recoverdm: $(OBJSr)
	$(CC) -Wall -W $(OBJSr) $(LDFLAGS) -o recoverdm

mergebad: $(OBJSm)
	$(CC) -Wall -W $(OBJSm) $(LDFLAGS) -o mergebad

install:
	cp recoverdm mergebad /usr/local/bin
	echo
	echo Oh, blatant plug: http://keetweej.vanheusden.com/wishlist.html

clean:
	rm -f $(OBJSr) $(OBJSm) recoverdm mergebad core

package: clean
	mkdir recoverdm-$(VERSION)
	cp *.c *.h *.1 Makefile readme.txt license.txt recoverdm-$(VERSION)
	tar czf recoverdm-$(VERSION).tgz recoverdm-$(VERSION)
	rm -rf recoverdm-$(VERSION)
