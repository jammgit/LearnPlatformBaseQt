main : apue.o main.o
	g++ -o main apue.o main.o -lpthread

main.o :
	g++ -std=c++11 -c main.c -lpthread -I/usr/include/c++/5/ 

apue.o :
	g++ -c apue.c


.PHONY clean:
clean:
	-rm *.o main