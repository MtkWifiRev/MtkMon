all: nexmon.so

nexmon.so: nexmon.o
	g++ -m64 -shared -o $@ $<

nexmon.o: nexmon.c
	g++ -std=c++11 -Wall -fno-rtti -Wno-literal-suffix -m64 -fPIC -I../nds32le-linux-glibc-v3_pre_compiled/lib/gcc/nds32le-linux/8.0.1/plugin/include/ -c -o $@ $<

clean:
	rm -f *.o *.so
