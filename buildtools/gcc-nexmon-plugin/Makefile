all: nexmon.so

nexmon.so: nexmon.o
	g++ -m64 -shared -o $@ $<

nexmon.o: nexmon.c
	g++ -std=c++11 -Wall -fno-rtti -Wno-literal-suffix -m64 -fPIC -I../nds32le-nolibc/nds32le-elf/lib/gcc/nds32le-elf/7.3.0/plugin/include -c -o $@ $<

clean:
	rm -f *.o *.so
