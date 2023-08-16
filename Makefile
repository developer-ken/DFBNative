libdfb.so: libdfb.c
	gcc -shared -fPIC -o libdfb.so libdfb.c

demo: libdfb.c
	gcc -o demo libdfb.c

clean:
	rm -f libdfb.so demo

install:
	cp libdfb.so /usr/lib

all: libdfb.so demo