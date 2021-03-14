myfind: myfind.o
	gcc myfind.o -o myfind

myfind.o: myfind.c
	gcc -c myfind.c

clean:
	rm *.o myfind