all: nexmon.so

nexmon.so: nexmon.o
	g++ -m64 -shared -o $@ $<

nexmon.o: nexmon.c
	g++ -std=c++11 -Wall -fno-rtti -Wno-literal-suffix -m64 -fPIC -I../gcc-12.2.0-nolibc/nds32le-linux/lib/gcc/nds32le-linux/12.2.0/plugin/include/ -c -o $@ $<

clean:
	rm -f *.o *.so
