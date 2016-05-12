all:hello

hello:hello.o
	gcc $^ -o $@
.c.o:
	gcc $^ -c -o $@
clean:
	rm *.o hello -fr
