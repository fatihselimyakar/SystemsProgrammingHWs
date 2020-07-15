#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <sys/stat.h>

#define BUFFERSIZE (64 * 1024)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

ssize_t write_lock(int fd,const void* buf,size_t byte);
ssize_t read_lock(int fd, void *buf, size_t count);
int write_with_no_overwrite(int fd, off_t offset, char const *insert, size_t inslen);
char* convert_char_to_complex(char* chars);
void read_and_write(char* inputPath,char* outputPath,int time);
int msleep(unsigned int tms);

/* Provides tms ms sleep */
int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

/* Writes with locking */
ssize_t write_lock(int fd,const void* buf,size_t byte){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_WRLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programA | write_lock | fcntl error lock");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if((ret_val= write(fd,buf,byte))<0 ){
        perror("programA | write_lock | write error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programA | write_lock | fcntl error unlock");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;
}

/* Reads with locking */
ssize_t read_lock(int fd, void *buf, size_t count){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_RDLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programA | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);
    

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programA | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;

}

/* Writes the file with no overwriting */
int write_with_no_overwrite(int fd, off_t offset, char const *insert, size_t inslen){
    char buffer[BUFFERSIZE];
    struct stat sb;
    int rc = -1;

    if (fstat(fd, &sb) == 0)
    {
        if (sb.st_size > offset)
        {
            /* Move data after offset up by inslen bytes */
            size_t bytes_to_move = sb.st_size - offset;
            off_t read_end_offset = sb.st_size; 
            while (bytes_to_move != 0)
            {
                ssize_t bytes_this_time = MIN(BUFFERSIZE, bytes_to_move);
                ssize_t rd_off = read_end_offset - bytes_this_time;
                ssize_t wr_off = rd_off + inslen;
                if(lseek(fd, rd_off, SEEK_SET)<0){
                    perror("programA | write_with_no_overwrite | lseek error");
                    close(fd);
                    exit(EXIT_FAILURE);
                }
                if (read_lock(fd, buffer, bytes_this_time) != bytes_this_time)
                    return -1;
                if(lseek(fd, wr_off, SEEK_SET)<0){
                    perror("programA | write_with_no_overwrite | lseek error");
                    close(fd); 
                    exit(EXIT_FAILURE);
                }
                if (write_lock(fd, buffer, bytes_this_time) != bytes_this_time)
                    return -1;
                bytes_to_move -= bytes_this_time;
                read_end_offset -= bytes_this_time;
            }   
        }   
        if(lseek(fd, offset, SEEK_SET)<0){
            perror("programA | write_with_no_overwrite | lseek error");
            close(fd);
            exit(EXIT_FAILURE);
        }
        write_lock(fd, insert, inslen);
        rc = 0;
    }   
    return rc;
}

/* Converts the given characters to complex numbers */
char* convert_char_to_complex(char* chars){
    int i;
    char *result=(char*)malloc(500);//for all 16 complex number
    char str[10];//for ascii returning
    for(i=0;chars[i]!='\0';++i){
        if(!(i%2)){
            if(i==30)
                sprintf(str, "%d+i%d\n", chars[i],chars[i+1]);
            else
                sprintf(str, "%d+i%d,", chars[i],chars[i+1]);
            strcat(result,str);
        }
    }
    if(i!=32){
        errno=EPERM;
        perror("programA | convert_char_to_complex");
        exit(EXIT_FAILURE);
    }

    return result;
}

/* Includes programA's whole progress */
void read_and_write(char* inputPath,char* outputPath,int time){
    /* opens the input file */
    int fd,fd2;
    int read_flag=1,read_flag2,exit_loop_flag,counter=0;
    char newline_flag;

    /* Opens the files for read and read-write mode */
    fd = open(inputPath, O_RDONLY | O_SYNC);
    fd2 = open(outputPath,O_RDWR | O_SYNC);
    if (fd < 0 || fd2 < 0)  
    { 
        perror("programA | read_and_write | open file error");
        exit(EXIT_FAILURE); 
    } 

    /* Reads the input file 1 or file 2 */
    while(read_flag>0){
        char buf[32];

        ++counter;
        /* reads 32 byte */
        read_flag=read_lock(fd,buf,32);
        if(read_flag==32){
            //FILE 3 PROCESSES
            char buf_file2[1];
            char* complex_numbers=convert_char_to_complex(buf);//freele
            
            /* sets exit_loop_flag for exiting the inner loop */
            exit_loop_flag=1;
            
            /* read file 2 if there is empty row then write */
            read_flag2=1;

            /* reads and if file is valid then writes */
            while(read_flag2>0 && exit_loop_flag){
                /* reads 1 by 1 */
                if((read_flag2=read_lock(fd2,buf_file2,1))<0){
                    perror("programA | read_and_write | read file error");
                    close(fd);
                    close(fd2); 
                    exit(EXIT_FAILURE);
                }
                else if(buf_file2[0]=='\f'){
                    if(write_with_no_overwrite(fd2, lseek(fd2,0,SEEK_CUR)-1, complex_numbers, strlen(complex_numbers))<0){
                            perror("programA | read_and_write | write_with_no_overwrite error");
                            close(fd);
                            close(fd2);
                            exit(EXIT_FAILURE);
                    }
                    else{
                        msleep(time);
                        if(lseek(fd2,1,SEEK_CUR)<0){//for avoid '\n' 
                            perror("programA | read_and_write | lseek error");
                            close(fd);
                            close(fd2); 
                            exit(EXIT_FAILURE);
                        }
                        exit_loop_flag=0;
                    }
                }
                /* first row that is '\n' */
                else if((read_flag2>0) && lseek(fd2,0,SEEK_CUR)==1 && buf_file2[0]=='\n'){
                    /* Writing with avoiding overwrite */ 
                    if(write_with_no_overwrite(fd2, lseek(fd2,0,SEEK_CUR)-1, complex_numbers, strlen(complex_numbers)-1)<0){
                            perror("programA | read_and_write | write_with_no_overwrite error");
                            close(fd);
                            close(fd2); 
                            exit(EXIT_FAILURE);
                    }
                    else{
                        msleep(time);
                        if(lseek(fd2,1,SEEK_CUR)<0){//for avoid '\n' 
                            perror("programA | read_and_write | lseek error");
                            close(fd);
                            close(fd2);
                            exit(EXIT_FAILURE);
                        }
                        exit_loop_flag=0;
                    }
                }
                /* except first row that is '\n' */
                else if((read_flag2>0) && newline_flag=='\n'){
                    if(buf_file2[0]=='\n'){
                        /* For write locking for file3 */
                        /* Writing with avoiding overwrite */
                        if(write_with_no_overwrite(fd2, lseek(fd2,0,SEEK_CUR)-1, complex_numbers, strlen(complex_numbers)-1)<0){
                            perror("programA | read_and_write | write_with_no_overwrite error"); 
                            close(fd);
                            close(fd2);
                            exit(EXIT_FAILURE);
                        }
                        else{
                            msleep(time);
                            if(lseek(fd2,1,SEEK_CUR)<0){//for avoid '\n' 
                                perror("programA | read_and_write | lseek error");
                                close(fd);
                                close(fd2); 
                                exit(EXIT_FAILURE);
                            }
                            exit_loop_flag=0;
                        }
                    }
                }
                /* a row that is empty */
                else if(read_flag2==0){
                    /* Writing */
                    if( write_lock(fd2,complex_numbers,strlen(complex_numbers))<0 ){
                        perror("programA | read_and_write | write error");
                        close(fd);
                        close(fd2);
                        exit(EXIT_FAILURE);
                    }
                    else{
                        msleep(time);
                        exit_loop_flag=0;
                    }
                }
                newline_flag=buf_file2[0];//Updates the newline_flag
            }
            free(complex_numbers);
        }
        else if(read_flag==0 && counter==1){
            errno=1;
            perror("programA | read_and_write | input file must not be empty "); 
            close(fd);
            close(fd2);
            exit(EXIT_FAILURE); 
        }
        else if(read_flag<0){
            perror("programA | read_and_write | read error ");
            close(fd);
            close(fd2);
            exit(EXIT_FAILURE); 
        }
    }
    if (close(fd) < 0 || close(fd2) < 0)  
    { 
        perror("programA | read_and_write | close file error "); 
        exit(EXIT_FAILURE); 
    } 

}

int main(int argc,char *argv[]){
    int opt;
    char* ivalue;
    char* ovalue;
    char* tvalue;

    while((opt = getopt(argc, argv, ":i:o:t:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                ivalue=optarg;
                break;
            case 'o':
                ovalue=optarg;
                break;
            case 't':  
                tvalue=optarg;
                /* Controls the time range */
                if(atoi(tvalue)<1||atoi(tvalue)>50){ 
                    errno=EINVAL;
                    perror("programA | main | invalid time range");
                    exit(EXIT_FAILURE);
                }
                break;
            case ':':  
                errno=EINVAL;
                perror("programA | main |You must write like this: \"./program -i inputPath -o outputPath -t time\"");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                perror("programA | main | You must write like this: \"./program -i inputPath -o outputPath -t time\"");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }  
    }  

    if(optind!=7){
        errno=EINVAL;
        perror("programA | main | You must write like this: \"./program -i inputPath -o outputPath -t time\"");
        exit(EXIT_FAILURE);
    }

    read_and_write(ivalue,ovalue,atoi(tvalue));


    //free(str);
    return 0;
}