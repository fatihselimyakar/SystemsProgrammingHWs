all : server client

server : server.o
	gcc  -o server server.o graph.o queue.o cache.o input_output.o -lm -pthread 

server.o : server.c
	gcc -c -g server.c graph.c queue.c cache.c input_output.c -Wall -Wextra -pedantic

client : client.o
	gcc  -o client client.o input_output.o -lm -pthread 

client.o : client.c
	gcc -c -g client.c input_output.c -Wall -Wextra -pedantic

clean:
	rm -f server *.o
