#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "input_output.h"

/* gets time according to microseconds */
unsigned long get_time_microseconds(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return 1000000 * tv.tv_sec + tv.tv_usec;;
}

/* Prints error in the STDERR */
void print_error(char error_message[]){
    if(write(STDERR_FILENO,error_message,strlen(error_message))<0){
        exit(EXIT_FAILURE);
    }
}

/* Prints string in the output_fd */
void print_string_output_fd(char string[],int output_fd){
    if(write(output_fd,string,strlen(string))<0){
        unlink("running");
        exit(EXIT_FAILURE);
    }
}

/* Prints string in the output_fd */
void print_string(char string[]){
    if(write(STDOUT_FILENO,string,strlen(string))<0){
        print_error("input_output | print_string | write error\n");
        exit(EXIT_FAILURE);
    }
}




