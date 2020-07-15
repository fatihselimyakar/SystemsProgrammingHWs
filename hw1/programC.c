#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <complex.h>

#define BUFFERSIZE (64 * 1024)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

float cumulative_magnitute_of_complex_numbers(char* complex_numbers);
int number_of_line_file(char* path);
int write_with_no_overwrite(int fd, off_t offset, char const *insert, size_t inslen);
ssize_t write_lock(int fd,const void* buf,size_t byte);
ssize_t read_lock(int fd, void *buf, size_t count);
int set_the_nth_line(int fd,int nth,char* line);
char* get_the_nth_line(int fd,int nth,int *offset);
void merge(int fd, int start, int mid, int end);
void mergeSort(int fd, int l, int r);


/* Calculates the total magnitute of given complex numbers line */
float cumulative_magnitute_of_complex_numbers(char* complex_numbers){
    char* str = strtok(complex_numbers,",+i\n");
    float result=0;
    float real=0;
    float imag=0;
    int i=0;

    while (str != NULL){   
        if(i%2==0)
            real=pow(atof(str),2);
        str = strtok (NULL, ",+i\n");
        if(i%2==0){
            imag=pow(atof(str),2);
            result=result+sqrt(real+imag);
        }
        ++i;
    }

    return result;
}

/* Returns the number of line in the file by counting '\n' */
int number_of_line_file(char* path){
    char buf[1];
    int newline_counter=0;
    int fd = open(path, O_RDONLY | O_SYNC);
    if(fd<0){
        perror("programC | number_of_line_file | fcntl error"); 
        exit(EXIT_FAILURE);
    }

    while(read_lock(fd,buf,1)>0){
        if(buf[0]=='\n')
            ++newline_counter;
    }

    if (close(fd) < 0)  
    { 
        perror("programC | number_of_line_file | close file error"); 
        exit(EXIT_FAILURE); 
    }

    return newline_counter;
}

/* Writes in file with extending */
int write_with_no_overwrite(int fd, off_t offset, char const *insert, size_t inslen){
    char buffer[BUFFERSIZE];
    struct stat sb;
    int rc = -1;
    struct flock lock;
    memset(&lock,0,sizeof(lock));

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
                    perror("programC | write_with_no_overwrite | lseek error");
                    close(fd); 
                    exit(EXIT_FAILURE);
                }
                if (read_lock(fd, buffer, bytes_this_time) != bytes_this_time)
                    return -1;
                if(lseek(fd, wr_off, SEEK_SET)<0){
                    perror("programC | write_with_no_overwrite | lseek error");
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
            perror("programC | write_with_no_overwrite | lseek error");
            close(fd);
            exit(EXIT_FAILURE);
        }
        write_lock(fd, insert, inslen);
        rc = 0;
    }   
    return rc;
}

/* Writes the file by using lock */
ssize_t write_lock(int fd,const void* buf,size_t byte){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_WRLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programC | write_lock | fcntl error lock");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    if((ret_val= write(fd,buf,byte))<0 ){
        perror("programC | write_lock | write error");
        close(fd);
        exit(EXIT_FAILURE);
    }
    

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programC | write_lock | fcntl error unlock");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;
}

/* Reads the file by using lock */
ssize_t read_lock(int fd, void *buf, size_t count){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_RDLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programC | read_lock | fcntl error"); 
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);
    

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programC | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;

}

/* Changes the nth line with given string in parameter */
int set_the_nth_line(int fd,int nth,char* line){//todo:FREELENCEK
    char *buf=(char*)malloc(1);
    int read_flag;
    int newline_counter=0;
    int total_length=strlen(line);
    int length_counter=0;
    int last_word_flag=1;
    int offset=0;
    if(lseek(fd,0,SEEK_SET)<0){
        perror("programC | set_the_nth_line | lseek error ");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    do{       
        read_flag=read_lock(fd,buf,1);
        if(*buf=='\n')
            ++newline_counter;
        else if(newline_counter==nth && length_counter<total_length){
            char s[1];
            s[0]=line[length_counter];
            offset=lseek(fd,-1,SEEK_CUR);
            write_lock(fd,s,1);
            ++length_counter;
            
        }
        if(newline_counter==nth && length_counter==total_length){
            if(last_word_flag==0){
                offset=lseek(fd,-1,SEEK_CUR);
                write_lock(fd,"\f",1);
            }
            last_word_flag=0;   
        }
    }
    while(read_flag>0);

    if(length_counter<total_length){
        for(int i=length_counter;i<total_length;++i){
            char extend_buf[1];
            extend_buf[0]=line[i];
            ++offset;
            write_with_no_overwrite(fd,offset,extend_buf,1);    
        }
    }

    free(buf);

    return 1;
}

/* Returns the nth line with the given fd */
char* get_the_nth_line(int fd,int nth,int *offset){//todo:FREELENCEK
    int read_flag;
    char *buf=(char*)malloc(1);
    int newline_counter=0;
    *offset=0;
    char* line_buffer=(char*)malloc(1024);
    if(lseek(fd,0,SEEK_SET)<0){
        perror("programC | get_the_nth_line | lseek error "); 
        close(fd);
        exit(EXIT_FAILURE);
    }
    do{
        read_flag=read_lock(fd,buf,1);
        if(*buf=='\n')
            ++newline_counter;
        else if(newline_counter==nth){
            strcat(line_buffer,buf);
        }
        if(newline_counter<nth)
            ++(*offset);
    }
    while(read_flag>0);
    strcat(line_buffer,"\0");//sonradan ekledim
    if(nth!=0)
        (*offset)+=1;
    free(buf);

    return line_buffer;
}

/* Merge sort's merge function */
void merge(int fd, int start, int mid, int end){ 
    int start2 = mid + 1; 
    int offset;

    if (cumulative_magnitute_of_complex_numbers(get_the_nth_line(fd,mid,&offset)) <= cumulative_magnitute_of_complex_numbers(get_the_nth_line(fd,start2,&offset))) { 
        return; 
    } 
  
    while (start <= mid && start2 <= end) { 
        if (cumulative_magnitute_of_complex_numbers(get_the_nth_line(fd,start,&offset)) <= cumulative_magnitute_of_complex_numbers(get_the_nth_line(fd,start2,&offset))) { 
            start++; 
        } 
        else { 
            char* value = get_the_nth_line(fd,start2,&offset); 
            int index = start2; 

            while (index != start) { 
                set_the_nth_line(fd,index,get_the_nth_line(fd,index-1,&offset));
                index--; 
            }
            set_the_nth_line(fd,start,value);

            start++; 
            mid++; 
            start2++; 
        } 
    } 
}

/* Merge sort for given fd respect to line's cumulative magnitude */
void mergeSort(int fd, int l, int r){ 
    if (l<r) { 
        int m=l+(r-l)/2; 

        mergeSort(fd,l,m); 
        mergeSort(fd,m+1,r); 
  
        merge(fd,l,m,r); 
    } 
} 

int main(int argc,char* argv[]){
    int opt;
    char* ivalue;

    while((opt = getopt(argc, argv, ":i:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                ivalue=optarg;
                break;
            case ':':  
                errno=EINVAL;
                perror("programC | main |You must write like this: \"./program -i inputPath \"");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                perror("programC | main | You must write like this: \"./program -i inputPath \"");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }  
    }  

    if(optind!=3){
        errno=EINVAL;
        perror("programC | main | You must write like this: \"./program -i inputPath \"");
        exit(EXIT_FAILURE);
    }

    int fd=open(ivalue,O_RDWR | O_SYNC);
    int offset=0;
    if(fd<0){
        perror("programC | main | Open file error");
        exit(EXIT_FAILURE);
    }

    mergeSort(fd,0,number_of_line_file(ivalue)-1);

    if(close(fd)<0){
        perror("programC | main | Close file error");
        exit(EXIT_FAILURE);
    }
}