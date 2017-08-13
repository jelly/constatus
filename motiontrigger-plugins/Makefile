# (C) 2017 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CXXFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags opencv`
VERSION=0.1

OBJS1=demo.o
OBJS2=demoocv.o

all: lib1 lib2

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

lib2: $(OBJS2)
	ar -r demoocv.a $(OBJS2)
	ld -g -x -lm `pkg-config --libs opencv` -shared -soname demoocv.so.2 -o demoocv.so.$(VERSION) --whole-archive demoocv.a

install: all
	cp demo.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.*

clean:
	rm -f $(OBJS1) demo.a demo.so.* core
