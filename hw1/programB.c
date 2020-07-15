#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <complex.h>

#define BUFFER_SIZE 1024
#define PI 3.141593
#define BUFFERSIZE (64 * 1024)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

ssize_t write_lock(int fd,const void* buf,size_t byte,int time,int offset);
ssize_t read_lock(int fd, void *buf, size_t count);
ssize_t normal_write_lock(int fd,const void* buf,size_t byte);
int write_with_no_overwrite(int fd, off_t offset, char const *insert, size_t inslen);
void FFT_inner(double complex buf[], double complex out[], int n, int step);
void FFT(double complex buf[], int n);
char* FFT_out(double complex buf[]);
char* split_and_calculate_FFT(char* complex_numbers);
int is_empty(char* path );
int number_of_line_file(int fd);
ssize_t read_line (char *buf, size_t sz, int fd, off_t *offset,int remove_line);
void write_output_file(char *output_path,char *output_string,int time);
int msleep(unsigned int tms);

int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

/* Writes the file without overwriting */
int write_with_no_overwrite(int fd, off_t offset, char const *insert, size_t inslen){
    char buffer[BUFFERSIZE];
    struct stat sb;
    int rc = -1;

    if (fstat(fd, &sb) == 0){
        if (sb.st_size > offset){
            size_t bytes_to_move = sb.st_size - offset;
            off_t read_end_offset = sb.st_size; 
            while (bytes_to_move != 0){
                ssize_t bytes_this_time = MIN(BUFFERSIZE, bytes_to_move);
                ssize_t rd_off = read_end_offset - bytes_this_time;
                ssize_t wr_off = rd_off + inslen;
                if(lseek(fd, rd_off, SEEK_SET)<0){
                    perror("programB | write_with_no_overwrite | lseek error");
                    close(fd);
                    exit(EXIT_FAILURE);
                }
                if (read_lock(fd, buffer, bytes_this_time) != bytes_this_time)
                    return -1;
                if(lseek(fd, wr_off, SEEK_SET)<0){
                    perror("programB | write_with_no_overwrite | lseek error");
                    close(fd); 
                    exit(EXIT_FAILURE);
                }
                if (normal_write_lock(fd, buffer, bytes_this_time) != bytes_this_time)
                    return -1;

                bytes_to_move -= bytes_this_time;
                read_end_offset -= bytes_this_time;
            }   
        }   
        if(lseek(fd, offset, SEEK_SET)<0){
            perror("programB | write_with_no_overwrite | lseek error");
            close(fd); 
            exit(EXIT_FAILURE);
        }
        normal_write_lock(fd, insert, inslen);
        rc = 0;
    }   
    return rc;
}

/* FFT calculator */
void FFT_inner(double complex buf[], double complex out[], int n, int step)
{
    int i;
	if (step < n) {
		FFT_inner(out, buf, n, step * 2);
		FFT_inner(out + step, buf + step, n, step * 2);
        
		for (i = 0; i < n; i += 2 * step) {
			double complex t = cexp(-I * PI * i / n) * out[i + step];
			buf[i / 2]     = out[i] + t;
			buf[(i + n)/2] = out[i] - t;
		}
	}
}

/* FFT calculate wrapper */
void FFT(double complex buf[], int n)
{
	double complex out[n];
    int i;
	for (i = 0; i < n; i++) 
        out[i] = buf[i];
 
	FFT_inner(buf, out, n, 1);
}

/* Converts the complex numbers to comma seperated 16 complex numbers */
char* FFT_out(double complex buf[]) {
    char *result=(char*)malloc(500);
    char str[50];//for ascii returning
    int i;
	for (i = 0; i < 16; i++){
        if (!cimag(buf[i]))
            if(i==15)
			    sprintf(str,"%.3f+i0\n", creal(buf[i]));
            else
                sprintf(str,"%.3f+i0,", creal(buf[i]));
		else
            if(i==15)
                if(cimag(buf[i])<0)
                    sprintf(str,"%.3f-i%.3f\n", creal(buf[i]), -cimag(buf[i]));
                else
                    sprintf(str,"%.3f+i%.3f\n", creal(buf[i]), cimag(buf[i]));
            else
                if(cimag(buf[i])<0)
                    sprintf(str,"%.3f-i%.3f,", creal(buf[i]), -cimag(buf[i]));
                else
                    sprintf(str,"%.3f+i%.3f,", creal(buf[i]), cimag(buf[i]));
                
        strcat(result,str);
    }
		
    return result;
}

/* calculates the FFT by looking complex_numbers */
char* split_and_calculate_FFT(char* complex_numbers){
    double complex complex_buffer[17];//BU NIYE 16 DEGILDE 17
    char* str = strtok(complex_numbers,",+i\n");
    int i=0;
    int j;
    while (str != NULL && i<32){   
        char *buf=(char*)malloc(50);
        strcat(buf,str);
        strcat(buf,".");
        str = strtok (NULL, ",+i\n");
        if(str!=NULL)
            strcat(buf,str);
        if(i%2==0){
            complex_buffer[i/2]=(complex double)atof(buf);
        }
            
        ++i;
        for(j=0;j<50;++j){
            buf[j]='\0';
        }
        free(buf);
    }
    FFT(complex_buffer,16);
    return FFT_out(complex_buffer);
}

/* Controls the file is empty */
int is_empty(char* path ){
    char buf[1];
    int ret_val=1;
    int fd = open(path, O_RDWR | O_SYNC);
    if(fd<0){
        perror("programB | is_empty | fcntl error "); 
        exit(EXIT_FAILURE);
    }

    while(read_lock(fd,buf,1)>0){
        if(buf[0]!='\n'&&buf[0]!='\f')
            ret_val=0;
    }

    if (close(fd) < 0)  
    { 
        perror("programB | is_empty | close file error "); 
        exit(EXIT_FAILURE); 
    }

    lseek(fd,0,SEEK_SET);
    return ret_val;
}

/* Provides us to number of lines in the file */
int number_of_line_file(int fd){
    char buf[1];
    int newline_counter=0;

    while(read_lock(fd,buf,1)>0){
        if(buf[0]=='\n')
            ++newline_counter;
    }
    lseek(fd,0,SEEK_SET);
    return newline_counter;
}

/* Write syscall with locking and seeking */
ssize_t write_lock(int fd,const void* buf,size_t byte,int time,int offset){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_WRLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programB | write_lock | fcntl error lock");
        close(fd);
        exit(EXIT_FAILURE);
    }
    /* Writing */
    if(lseek(fd,offset,SEEK_DATA)<0){//for avoid '\n' 
        perror("programB | write_lock | lseek error");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if((ret_val= write(fd,buf,byte))<0 ){
        perror("programB | write_lock | write error");
        close(fd);
        exit(EXIT_FAILURE);
    }

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programB | write_lock | fcntl error unlock");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return ret_val;
}

/* Write syscall with only locking */
ssize_t normal_write_lock(int fd,const void* buf,size_t byte){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_WRLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programB | normal_write_lock | fcntl error lock");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    if((ret_val= write(fd,buf,byte))<0 ){
        perror("programB | normal_write_lock | write error");
        close(fd);
        exit(EXIT_FAILURE);
    }

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programB | normal_write_lock | fcntl error unlock");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;
}

/* Read syscall with only locking */
ssize_t read_lock(int fd, void *buf, size_t count){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_RDLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programB | read_lock | fcntl error "); 
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("programB | read_lock | fcntl error ");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;

}

/* Reads the 1 line after the offset. */
ssize_t read_line (char *buf, size_t sz, int fd, off_t *offset,int remove_line){
    ssize_t nchr = 0;
    ssize_t index = 0;
    char *p = NULL;
    struct flock lock;

    if ((nchr=lseek (fd, *offset, SEEK_SET))!=-1)
        nchr=read_lock(fd, buf, sz);
    if (nchr == -1) {
        perror("programB | read_line | read error");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    if (nchr == 0) 
        return -1;

    p = buf; 
    while (index < nchr && *p != '\n') p++, index++;
    *p = 0;

    if (index == nchr) { 
        *offset += nchr;
        return nchr < (ssize_t)sz ? nchr : 0;
    }

    *offset += index + 1;
    /* Removing the line section */
    if(remove_line){
        char buffer[1];
        if(lseek(fd,*offset-1,SEEK_SET)<0){
            perror("programB | read_line | 1 lseek error lock");
            close(fd);
            exit(EXIT_FAILURE);
        }
        while(read_lock(fd,buffer,1)>0){
            if(lseek(fd,-(index+1),SEEK_CUR)<0){
                perror("programB | read_line | 1 lseek error lock");
                close(fd);
                exit(EXIT_FAILURE);
            }
            normal_write_lock(fd,buffer,1);
            if(lseek(fd,+(index),SEEK_CUR)<0){
                perror("programB | read_line | 2 lseek error lock");
                close(fd);
                exit(EXIT_FAILURE);
            }
        }
        lock.l_type=F_WRLCK;
        //lock.l_whence=SEEK_CUR;
        if(fcntl(fd,F_SETLKW,&lock)<0){
            perror("programB | read_line | fcntl error lock");
            close(fd);
            exit(EXIT_FAILURE);
        }
        if(ftruncate(fd,lseek(fd,0,SEEK_CUR)-index)<0){
            perror("programB | read_line | truncate error");
            close(fd); 
            exit(EXIT_FAILURE);
        }
        lock.l_type=F_UNLCK;
        //lock.l_whence=SEEK_CUR;
        if(fcntl(fd,F_SETLKW,&lock)<0){
            perror("programB | read_line | fcntl error lock");
            close(fd);
            exit(EXIT_FAILURE);
        }
        (*offset)-=index;
    }

    return index;
}

/* Holds to programB's write progress */
void write_output_file(char *output_path,char *output_string,int time){
    int read_flag;
    char buf_file[1];
    int fd=open(output_path,O_RDWR | O_SYNC);
    int newline_counter=0;
    int exit_flag=1;
    if(fd<0){ 
        perror("programB | write_output_file | open file error"); 
        exit(EXIT_FAILURE); 
    }

    do{
        if((read_flag=read_lock(fd,buf_file,1))<0){
            perror("programB | write_output_file | read error");
            close(fd);
            exit(EXIT_FAILURE);
        }
        if(buf_file[0]=='\n')
            ++newline_counter;
        else
            newline_counter=0;

        if(read_flag==0){
            if(write_with_no_overwrite(fd, lseek(fd,0,SEEK_CUR), output_string, strlen(output_string))<0){
                    perror("programB | write_output_file | write_with_no_overwrite error");
                    close(fd); 
                    exit(EXIT_FAILURE);
            }
            else{
                msleep(time);
                if(lseek(fd,1,SEEK_CUR)<0){//for avoid '\n' 
                    perror("programB | write_output_file | lseek error");
                    close(fd); 
                    exit(EXIT_FAILURE);
                }
                exit_flag=0;
            }
        }
        else if(newline_counter>=2){
            if(write_with_no_overwrite(fd, lseek(fd,0,SEEK_CUR)-1, output_string, strlen(output_string)-1)<0){
                    perror("programB | write_output_file | write_with_no_overwrite error");
                    close(fd);
                    exit(EXIT_FAILURE);
            }
            else{
                msleep(time);
                if(lseek(fd,1,SEEK_CUR)<0){//for avoid '\n' 
                    perror("programB | write_output_file | lseek error");
                    close(fd); 
                    exit(EXIT_FAILURE);
                }
                exit_flag=0;
            }
        }
        else if(buf_file[0]=='\n'&& lseek(fd,0,SEEK_CUR)==1){
            if(write_with_no_overwrite(fd, lseek(fd,0,SEEK_CUR)-1, output_string, strlen(output_string)-1)<0){
                    perror("programB | write_output_file | write_with_no_overwrite error");
                    close(fd); 
                    exit(EXIT_FAILURE);
            }
            else{
                msleep(time);
                if(lseek(fd,1,SEEK_CUR)<0){//for avoid '\n' 
                    perror("programB | write_output_file | lseek error");
                    close(fd); 
                    exit(EXIT_FAILURE);
                }
                exit_flag=0;
            }
        }
        
    }
    while(read_flag>0 && exit_flag);

    //free(output_string);

    if(close(fd)<0){
        perror("programB | write_output_file | close file error"); 
        exit(EXIT_FAILURE); 
    }

}

int main(int argc,char *argv[]){
    int opt;
    int fd;
    int random_num;
    int nof_line;
    char line[BUFFERSIZE]={0};
    off_t offset = 0;
    ssize_t len = 0;
    size_t i = 0;
    struct stat stats;
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
                    perror("programB | main | invalid time range");
                    exit(EXIT_FAILURE);
                }
                break;
            case ':':  
                errno=EINVAL;
                perror("programB | main |You must write like this: \"./program -i inputPath -o outputPath -t time\"");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                perror("programB | main | You must write like this: \"./program -i inputPath -o outputPath -t time\"");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    if(optind!=7){
        errno=EINVAL;
        perror("programB | main | You must write like this: \"./program -i inputPath -o outputPath -t time\"");
        exit(EXIT_FAILURE);
    }

    if((fd = open(ivalue, O_RDWR | O_SYNC))<0){
        perror("programB | main | open file error");
        exit(EXIT_FAILURE);
    }

    /* random line choice */
    srand(time(0));
    if((nof_line=number_of_line_file(fd))==0)
        random_num=0;
    else
        random_num=rand()%nof_line;

    do{
        /* If the file did not have use then waits */
        if (number_of_line_file(fd)==0){
            msleep(atoi(tvalue));
        }
        if(i+1<random_num)
            len = read_line (line, BUFFERSIZE, fd, &offset,0);
        else{
            len = read_line (line, BUFFERSIZE, fd, &offset,1);
            if(len!=0 && len!=-1 && line[0]!=0)
                write_output_file(ovalue,split_and_calculate_FFT(line),atoi(tvalue));
        }

        /* Returns the head of file and continues */
        if((i==number_of_line_file(fd))&&(!is_empty(ivalue))){
            offset=0;
            random_num=0;
            len=0;
            lseek(fd,0,SEEK_SET);
        }

        /* Avoids the while programA is writing when programB finish */
        if(is_empty(ivalue)){
            msleep(100);
            if(!is_empty(ivalue)){
                offset=0;
                random_num=0;
                len=0;
                lseek(fd,0,SEEK_SET);
            }
        }
        ++i;
    }
    while (len!= -1);

    if(ftruncate(fd,number_of_line_file(fd))<0){
        perror("programB | main | truncate error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    if(close(fd)<0){
        perror("programB | main | close file error");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}  