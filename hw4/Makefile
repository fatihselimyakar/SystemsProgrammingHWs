all : program

program : program.o
	gcc -o program program.o -lm -pthread -Wall -Wextra -pedantic

program.o : program.c
	gcc -c program.c 

clean:
	rm -f program *.o
