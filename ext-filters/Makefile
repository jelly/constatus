INSTALL_PREFIX=/usr/local

CFLAGS=-fPIC -Wall -pedantic
CXXFLAGS=-fPIC -Wall -pedantic
VERSION=0.1

OBJS1=demo.o
OBJS2=demo2.o

all: lib1 lib2

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

lib2: $(OBJS2)
	ar -r demo2.a $(OBJS2)
	ld -x -lm -shared -soname demo2.so.2 -o demo2.so.$(VERSION) --whole-archive demo2.a

install: all
	cp demo.so.* demo2.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.* $(INSTALL_PREFIX)/lib/demo2.so.*

clean:
	rm -f $(OBJS1) $(OBJS2) demo.a demo.so.* core demo2.a demo2.so.*
