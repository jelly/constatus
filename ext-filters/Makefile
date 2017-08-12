INSTALL_PREFIX=/usr/local

CFLAGS=-fPIC -Wall -pedantic
VERSION=0.1

OBJS=demo.o

all: lib

lib: $(OBJS)
	ar -r demo.a $(OBJS)
	ld -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

install: all
	cp demo.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.* 

clean:
	rm -f $(OBJS) demo.a demo.so.* core
