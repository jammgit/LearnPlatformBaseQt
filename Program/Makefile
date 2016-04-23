Main : Apue.o Main.o
	g++ -o Main Apue.o Main.o -lpthread -lmysqlclient

Main.o :
	g++ -std=c++11 -c Main.c -I/usr/include/c++/5/ -I/usr/include/mysql/

Apue.o :
	g++ -c Apue.c


.PHONY clean:
clean:
	-rm *.o Main