/****************************************************/
/* Author     : Fatih Selim Yakar                   */
/* ID         : 161044054                           */
/* Title      : Systems Programming Final - Client  */
/* Instructor : Erchan Aptoula                      */
/****************************************************/

#include <netdb.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "input_output.h"

/* Sends request to get path */
void send_request(int sockfd,int source,int destination){ 
    char buf[999999];
    char* will_print=(char*)malloc(1200000*sizeof(char));
    unsigned long response_time;
    sprintf(will_print,"[%lu]Client (%d) connected and requesting a path from node %d to %d\n",get_time_microseconds(),getpid(),source,destination);
    print_string(will_print);
    
    sprintf(buf,"%d->%d",source,destination);

    /* Measures the time */
    response_time=get_time_microseconds();

    /* Writes the requested path */
    write(sockfd, buf, strlen(buf)); 
    bzero(buf, sizeof(buf)); 
    /* Reads the response */
    read(sockfd, buf, sizeof(buf));
    if(strcmp(buf,"")==0){
        return;
    }
    /* If path was not found */
    if(strcmp(buf,"NO PATH")==0){
        sprintf(will_print,"[%lu]Server’s response (%d): NO PATH, arrived in %f seconds, shutting down.\n",get_time_microseconds(),getpid(),((double)get_time_microseconds()-response_time)/1000000);
        print_string(will_print);
    }
    /* If path was found */
    else{
        sprintf(will_print,"[%lu]Server’s response to (%d): %s, arrived in %f seconds.\n",get_time_microseconds(),getpid(),buf,((double)get_time_microseconds()-response_time)/1000000);
        print_string(will_print);
    }
    free(will_print);
} 
  
int main(int argc,char *argv[]){ 
    int sockfd; 
    struct sockaddr_in server_addr; 
    int opt;
    char will_print[500],server_addres[500];
    int port,source,destination,i;

    while((opt = getopt(argc, argv, ":a:p:s:d:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'a':
                strcpy(server_addres,optarg);
                break;
            case 'p':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57){
                        print_error("client | main | -p flag must be non negative number\n");
                        exit(EXIT_FAILURE);
                    }
                }
                port=atoi(optarg);
                break;
            case 's':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57){
                        print_error("client | main | -s flag must be non negative number\n");
                        exit(EXIT_FAILURE);
                    }
                }
                source=atoi(optarg);
                break;
            case 'd':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57){
                        print_error("client | main | -d flag must be non negative number\n");
                        exit(EXIT_FAILURE);
                    }
                }
                destination=atoi(optarg);
                break;
            case ':':  
                errno=EINVAL;
                print_error("client | main | You must write like this: \"./client -a ADDRES -p PORT -s SOURCE -d DESTINATION\"\n");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                print_error("client | main | You must write like this: \"./client -a ADDRES -p PORT -s SOURCE -d DESTINATION\"\n");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    /* Controls are there more or less parameters */
    if(optind!=9){
        errno=EINVAL;
        print_error("client | main | You must write like this: \"./client -a ADDRES -p PORT -s SOURCE -d DESTINATION\"\n");
        exit(EXIT_FAILURE);
    }
  
    /* Socket creation */
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        print_error("client | main | socket creation failed.\n");
	perror("");
        exit(EXIT_FAILURE); 
    } 
    bzero(&server_addr, sizeof(server_addr)); 
  
    // assign IP, PORT 
    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = inet_addr(server_addres); 
    server_addr.sin_port = htons(port); 
  
    /* connect the client socket to server socket */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) { 
        print_error("client | main | connect failed.\n");
	perror("");
        exit(EXIT_FAILURE);  
    } 
    else{
        sprintf(will_print,"[%lu]Client(%d) connecting to %s:%d\n",get_time_microseconds(),getpid(),server_addres,port); 
        print_string(will_print);
    }
        
  
    /* function for request */
    send_request(sockfd,source,destination);
  
    /* close the socket */
    close(sockfd); 
} 
