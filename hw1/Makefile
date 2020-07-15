all : programA programB programC

programC : programC.o
	gcc -o programC programC.o -lm

programB : programB.o
	gcc -o programB programB.o -lm

programA : programA.o
	gcc -o programA programA.o

programC.o : programC.c
	gcc -c programC.c 

programB.o : programB.c
	gcc -c programB.c

programA.o : programA.c
	gcc -c programA.c

clean:
	rm -f programA programB programC *.o