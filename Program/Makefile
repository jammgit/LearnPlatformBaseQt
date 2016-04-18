Main : Apue.o Main.o
	g++ -o Main Apue.o Main.o -lpthread

Main.o :
	g++ -std=c++11 -c Main.c -lpthread -I/usr/include/c++/5/ 

Apue.o :
	g++ -c Apue.c


.PHONY clean:
clean:
	-rm *.o Main