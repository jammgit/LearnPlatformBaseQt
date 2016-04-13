main : apue.o main.o
	g++ -o main apue.o main.o

main.o :
	g++ -c main.c

apue.o :
	g++ -c apue.c



.PHONY clean:
clean:
	-rm *.o main