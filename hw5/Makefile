all : floristApp

floristApp : floristApp.o
	gcc -o floristApp floristApp.o -lm -pthread 

floristApp.o : floristApp.c
	gcc -c -g floristApp.c -Wall -Wextra -pedantic

clean:
	rm -f floristApp *.o
