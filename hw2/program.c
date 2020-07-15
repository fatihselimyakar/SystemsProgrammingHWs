#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BUFFERSIZE (1024)

ssize_t read_line (char *buf, size_t sz, int fd, off_t *offset,int remove_line);
void signal_array_catcher(int signo);
char* least_square_method(char string[]);
double standart_dev(int len,double arr[]);
char* mean_errors(char string[]);
ssize_t read_lock(int fd, void *buf, size_t count);
ssize_t write_lock(int fd,const void* buf,size_t byte);
void read_input_file(int input_fd,int temp_fd,int pid);
void read_temp_file(int temp_fd,int output_fd);
void catcher(int signum);


int glob_fd=-1,glob_fd2=-1,glob_fd3=-1,glob_fd4=-1;
char temp_file[20],input_file[20];
int signal_array[100];
int signal_idx=-1;
sig_atomic_t sigusr1_flag=0,sigusr2_flag=0;

/* Reads the 1 line after the offset. */
ssize_t read_line (char *buf, size_t sz, int fd, off_t *offset,int remove_line){
    ssize_t nchr = 0;
    ssize_t index = 0;
    char *p = NULL;
    struct flock lock;

    if ((nchr=lseek (fd, *offset, SEEK_SET))!=-1)
        nchr=read_lock(fd, buf, sz);
    if (nchr == -1) {
        perror("program | read_line | read error");
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
            perror("program | read_line | 1 lseek error lock");
            close(fd);
            exit(EXIT_FAILURE);
        }
        while(read_lock(fd,buffer,1)>0){
            if(lseek(fd,-(index+1),SEEK_CUR)<0){
                perror("program | read_line | 1 lseek error lock");
                close(fd);
                exit(EXIT_FAILURE);
            }
            write_lock(fd,buffer,1);
            if(lseek(fd,+(index),SEEK_CUR)<0){
                perror("program | read_line | 2 lseek error lock");
                close(fd);
                exit(EXIT_FAILURE);
            }
        }
        if(ftruncate(fd,lseek(fd,0,SEEK_CUR)-index)<0){
            perror("program | read_line | truncate error");
            close(fd); 
            exit(EXIT_FAILURE);
        }
        (*offset)-=index;
    }

    return index;
}

/* Saves the signal that caught in critical section in the signal_array */
void signal_array_catcher(int signo){
    ++signal_idx;
    signal_array[signal_idx]=signo;
}

/* Calculates the fit line equation by using least square method */
char* least_square_method(char string[]){
    /* sigaction setup to catch coming signals in critical section */
    struct sigaction sact_main;
    sigemptyset(&sact_main.sa_mask); 
    sact_main.sa_flags = 0;
    sact_main.sa_handler = signal_array_catcher;

    /* mask setup to block for creating critical section */
    sigset_t int_stp_mask;
    if((sigemptyset(&int_stp_mask)==-1) || (sigaddset(&int_stp_mask,SIGINT)==-1) || (sigaddset(&int_stp_mask,SIGSTOP)==-1)){
        perror("program | least_square_method | failed to initialize the mask");
        exit(EXIT_FAILURE);
    }

    /* Creates the critical section with sigprocmask */
    if(sigprocmask(SIG_BLOCK,&int_stp_mask,NULL)==-1){
        perror("program | least_square_method | failed to blocking the mask");
        exit(EXIT_FAILURE);
    }
    int i;
    /* handles all of signals in critical section */
    for(i=1;i<32;++i){
        if(i!=SIGKILL && i!=SIGSTOP){
            if(sigaction(i,&sact_main,NULL)!=0){
                perror("Any signal sigaction() error");
                exit(EXIT_FAILURE);
            }    
        }   
    }
    
    /* least square method calculation section */
    double sum_xy=0;
    double sum_x=0;
    double sum_y=0;
    double sum_x2=0;
    double a=0;
    double b=0;
    char ret_eq[20];
    char* ret_val=(char *)malloc(250*sizeof(char));

    if(strlen(string)!=20){
        errno=EPERM;
        perror("program | least_square_method | string length is not 20");
        exit(EXIT_FAILURE);
    }
    for(i=0;i<20;i+=2){
        char *add_str=(char *)malloc(20*sizeof(char));
        unsigned int x_int,y_int;
        x_int=(unsigned char)string[i];
        y_int=(unsigned char)string[i+1];
        sum_xy+=x_int*y_int;
        sum_x+=x_int;
        sum_y+=y_int;
        sum_x2+=x_int*x_int;
        sprintf(add_str,"(%u, %u), ",x_int,y_int);
        strcat(ret_val,add_str);
        free(add_str);
    }
    a=((10*sum_xy)-(sum_x*sum_y))/((10*sum_x2)-(sum_x*sum_x));
    b=(sum_y-(a*sum_x))/10;
    /* Calculated equations */
    if(b<0)
        sprintf(ret_eq,"%.3fx-%.3f\n",a,b);
    else
        sprintf(ret_eq,"%.3fx+%.3f\n",a,b);

    /* concatanate points and their equation */
    strcat(ret_val,ret_eq);

    /* End of the critical section */
    if(sigprocmask(SIG_UNBLOCK,&int_stp_mask,NULL)==-1){
        perror("program | least_square_method | failed to un-blocking the mask");
        exit(EXIT_FAILURE);
    }

    /* returns (x_1, y_1),....,(x_10, y_10), ax+b type string */
    return ret_val;

}

/* Calculates standart deviation by dividing N */
double standart_dev(int len,double arr[]){
    double sum = 0;
    int i=0; 
    for(i=0;i< len; i++) 
        sum += arr[i]; 
    double mean = (double)sum / (double)len; 

    double sqDiff = 0; 
    for(int i = 0; i < len; i++)  
        sqDiff += (arr[i] - mean) * (arr[i] - mean); 

    return sqrt(sqDiff / len);
}

/* Calculates mean errors in critical section and returns the concatanated string */
char* mean_errors(char string[]){
    /* sets the sigset_t for blocking and then adds the SIGINT and SIGSTOP in the set */
    sigset_t int_stp_mask;
    if((sigemptyset(&int_stp_mask)==-1) || (sigaddset(&int_stp_mask,SIGINT)==-1) || (sigaddset(&int_stp_mask,SIGSTOP)==-1)){
        perror("program | mean_errors | failed to initialize the mask");
        exit(EXIT_FAILURE);
    }

    /* Blocks the created set */
    if(sigprocmask(SIG_BLOCK,&int_stp_mask,NULL)==-1){
        perror("program | mean_errors | failed to blocking the mask");
        exit(EXIT_FAILURE);
    }
    
    /* Calculate section */
    int i=0;
    double numbers[22];
    double MAE=0;
    double MSE=0;
    double RMSE=0;
    double error_array[3];
    char* ret_val=malloc(200*sizeof(char));
    char* errors=malloc(20*sizeof(char));
    strcpy(ret_val,string);

    /* split the string and creates the numbers array */
    char* str = strtok(string,"( ),x\n\0");
    while (str != NULL){   
        if(i%2==0)
            numbers[i]=atof(str);
        str=strtok(NULL,"( ),x\n\0");
        if(str!=NULL && i%2==0)
            numbers[i+1]=atof(str);

        ++i;
    }
    /* calculates with implementing the formulas */
    for(i=0;i<20;i+=2){
        MAE+=fabs((numbers[i]*numbers[20])-(numbers[i+1])+numbers[21])/(sqrt(numbers[20]*numbers[20]+1));
        MSE+=pow(fabs((numbers[i]*numbers[20])-(numbers[i+1])+numbers[21])/(sqrt(numbers[20]*numbers[20]+1)),2);
    }

    MAE/=10;
    MSE/=10;
    RMSE=sqrt(MSE);

    sprintf(errors," ,%.3f, %.3f, %.3f\n",MAE,MSE,RMSE);
    strcat(ret_val,errors);
    free(errors);

    error_array[0]=MAE; error_array[1]=MSE; error_array[2]=RMSE; 
    char print_string[100];
    sprintf(print_string,"MAE=%.3f MSE=%.3f RMSE=%.3f Mean=%.3f Standart Dev=%.3f\n",MAE,MSE,RMSE,(MAE+MSE+RMSE)/3,standart_dev(3,error_array));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        perror("program | failed in writing STDOUT.\n");
        exit(EXIT_FAILURE);
    }

    /* Unblocks and finishes the critical section */
    if(sigprocmask(SIG_UNBLOCK,&int_stp_mask,NULL)==-1){
        perror("program | mean_errors | failed to un-blocking the mask");
        exit(EXIT_FAILURE);
    }

    /* Returns the (x_1, y_1),....,(x_10, y_10), ax+b, MAE, MSE, RMSE type string */
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
        perror("program | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);
    

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("program | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;

}

/* Writes with locking */
ssize_t write_lock(int fd,const void* buf,size_t byte){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_WRLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("program | write_lock | fcntl error lock");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if((ret_val= write(fd,buf,byte))<0 ){
        perror("program | write_lock | write error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    lock.l_type=F_UNLCK;
    //lock.l_whence=SEEK_CUR;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        perror("program | write_lock | fcntl error unlock");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;
}

/* Parent's function */
/* reads the input files 20 by 20 bytes and writes the calculated equation and coordinates in template file */
void read_input_file(int input_fd,int temp_fd,int pid){
    ssize_t read_flag=0;
    ssize_t read_bytes=0;
    int lines=0,i=0;

    do{
        char *buf=malloc(20*sizeof(char));
        read_flag=read_lock(input_fd,buf,20);
        read_bytes+=read_flag;
        if(read_flag==20){
            char* calculated=least_square_method(buf);
            write_lock(temp_fd,calculated,strlen(calculated));

            ++lines;
            free(calculated);
        }
        else if(read_flag<0){
            perror("program | read_input_file | read error ");
            close(input_fd); 
            close(temp_fd); 
            exit(EXIT_FAILURE); 
        }
        free(buf);
        /* in first write send the signal from itselft to child process */
        if(i==0){
            kill(pid,SIGUSR2);
            char print_string[]="SIGUSR2 signal sent.\n";
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                perror("program | failed in writing STDOUT.\n");
                close(input_fd); 
                close(temp_fd); 
                exit(EXIT_FAILURE);
            }
        }
        ++i;
    }
    while(read_flag>0);

    char print_string[100];
    /* After the read and write in parent process prints the above informations on terminal */
    sprintf(print_string,"Parent read bytes:%lu\nNumber of line estimated equations:%d\n",read_bytes,lines);
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        perror("program | failed in writing STDOUT.\n");
        close(input_fd); 
        close(temp_fd); 
        exit(EXIT_FAILURE);
    }

    if(signal_idx==-1){
        sprintf(print_string,"There were not get any signal in critical sections.\n");
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            close(input_fd); 
            close(temp_fd); 
            exit(EXIT_FAILURE);
        }
    }
    else{
        sprintf(print_string,"There were %d signal in critical sections\nThe signals are: ",signal_idx+1);
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            close(input_fd); 
            close(temp_fd); 
            exit(EXIT_FAILURE);
        }
        for(int i=0;i<signal_idx+1;++i){
            sprintf(print_string,"%d ",signal_array[i]);
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                perror("program | failed in writing STDOUT.\n");
                close(input_fd); 
                close(temp_fd); 
                exit(EXIT_FAILURE);
            }
        }
        sprintf(print_string,"\n");
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            close(input_fd); 
            close(temp_fd); 
            exit(EXIT_FAILURE);
        }
    }
   
}

/* Child's function */
/* Reads the temp file and writes the output with errors */
void read_temp_file(int temp_fd,int output_fd){
    off_t offset=0;
    char line[BUFFERSIZE];
    int len=0,first_len=0,i=0;
    do{
        len=read_line (line, BUFFERSIZE, temp_fd, &offset,1);
        if(len>80){
            char *string=mean_errors(line);
            write_lock(output_fd,string,strlen(string));
            free(string);
        }
        ++i;
    }
    while (len!= -1);
}

/* signal handler function */
void catcher(int signum) {
    char print_string[100];   
    switch (signum) {
            case SIGUSR1: 
                sprintf(print_string,"catcher caught SIGUSR1\n");
                if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                    perror("program | failed in writing STDOUT.\n");
                    exit(EXIT_FAILURE);
                }
                sigusr1_flag=1;
                break;
            case SIGUSR2: 
                sprintf(print_string,"catcher caught SIGUSR2\n");
                if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                    perror("program | failed in writing STDOUT.\n");
                    exit(EXIT_FAILURE);
                }
                sigusr2_flag=1;
                break;
            case SIGTERM: 
                sprintf(print_string,"catcher caught SIGTERM\n");
                if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                    perror("program | failed in writing STDOUT.\n");
                    exit(EXIT_FAILURE);
                }
                if(glob_fd!=-1){
                    if (close(glob_fd) < 0){ 
                        perror("program | catcher | close file error"); 
                        exit(EXIT_FAILURE); 
                    }
                }
                if(glob_fd2!=-1){
                    if (close(glob_fd2) < 0){ 
                        perror("program | catcher | close file error"); 
                        exit(EXIT_FAILURE); 
                    }
                }
                if(glob_fd3!=-1){
                    if (close(glob_fd3) < 0){ 
                        perror("program | catcher | close file error"); 
                        exit(EXIT_FAILURE); 
                    }
                }
                if(glob_fd4!=-1){
                    if (close(glob_fd4) < 0){ 
                        perror("program | catcher | close file error"); 
                        exit(EXIT_FAILURE); 
                    }
                }
                if(strlen(temp_file)>0)
                    if(unlink(temp_file)<0){
                        perror("program | main | unlink error"); 
                        exit(EXIT_FAILURE); 
                    }

                if(strlen(input_file)>0)
                    if(unlink(input_file)<0){
                        perror("program | main | unlink error"); 
                        exit(EXIT_FAILURE); 
                    }
                exit(EXIT_SUCCESS);
                break;
            default:   
                sprintf(print_string,"catcher caught unexpected signal %d\n",signum);
                if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                    perror("program | failed in writing STDOUT.\n");
                    exit(EXIT_FAILURE);
                } 
        } 
    
}

int main(int argc,char *argv[]){
    int opt;
    int fd,input_fd,fd2,output_fd;
    int parent_read_bytes;
    pid_t pid;
    sigset_t sigset;
    struct sigaction sact;
    struct sigaction sact_main;
    sigemptyset(&sact_main.sa_mask); 
    sact_main.sa_flags = 0;
    sact_main.sa_handler = catcher;
    char print_string[100];

    /* if handles the sigterm than runs the catcher */
    if(sigaction(SIGTERM,&sact_main,NULL)!=0){
        perror("sigterm sigaction() error");
        exit(EXIT_FAILURE);
    }

    char ivalue[20];
    char ovalue[20];

    while((opt = getopt(argc, argv, ":i:o:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                strcpy(ivalue,optarg);
                break;
            case 'o':
                strcpy(ovalue,optarg);
                break;
            case ':':  
                errno=EINVAL;
                perror("program | main |You must write like this: \"./program -i inputPath -o outputPath\"");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                perror("program | main | You must write like this: \"./program -i inputPath -o outputPath\"");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    if(optind!=5){
        errno=EINVAL;
        perror("program | main | You must write like this: \"./program -i inputPath -o outputPath\"");
        exit(EXIT_FAILURE);
    }

    /* creates the temp_file */
    strcpy(temp_file,"tempXXXXXX");
    strcpy(input_file,ivalue);

    glob_fd=fd=mkstemp(temp_file);
    if(fd<0){
        perror("program | main | mkstemp error");
        exit(EXIT_FAILURE); 
    }

    /* sets the handler for sigusr1 and sigusr2 */
    sigemptyset(&sact.sa_mask); 
    sact.sa_flags = 0;
    sact.sa_handler = catcher;

    if (sigaction(SIGUSR2, &sact, NULL) != 0){
        perror("sigusr2 sigaction() error");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &sact, NULL) != 0){
        perror("sigusr1 sigaction() error");
        exit(EXIT_FAILURE);
    }

    /* forks and creates and new process */
    if((pid=fork())==0){
        /* Child process */
        sprintf(print_string,"***Child process started***\n");  
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            exit(EXIT_FAILURE);
        }

        /* waits the SIGUSR2 signal for continue */
        if(sigusr2_flag==0){
            sigfillset(&sigset);
            sigdelset(&sigset, SIGUSR2);
            sigsuspend(&sigset);
        }

        /* open output file */
        glob_fd2=output_fd=open(ovalue,O_WRONLY);
        if(output_fd<0){
            perror("program | main | open file error 1");
            exit(EXIT_FAILURE); 
        }

        /* open temp file */
        glob_fd3=fd2=open(temp_file,O_RDWR);
        if(fd2<0){
            perror("program | main | open file error 2");
            exit(EXIT_FAILURE); 
        }

        /* reads the temp file and writes the processed file in output file */
        read_temp_file(fd2,output_fd);
        
        /* waits the finish SIGUSR1 */
        if( sigusr1_flag ==0){
            sigfillset(&sigset);
            sigdelset(&sigset, SIGUSR1);
            sigsuspend(&sigset);
        }

        /* close the temp file */
        if (close(fd2) < 0){ 
            perror("program | main | close file error 3"); 
            exit(EXIT_FAILURE); 
        }

        /* close the output file */
        if (close(output_fd) < 0){ 
            perror("program | main | close file error 4"); 
            exit(EXIT_FAILURE); 
        } 

        /* removes the temp and input file */
        if((unlink(temp_file)<0 || unlink(ivalue)<0)){
            perror("program | main | unlink error"); 
            exit(EXIT_FAILURE); 
        }
        sprintf(print_string,"***Child process finished***\n");
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if(pid>0){
        /* Parent process */
        sprintf(print_string,"***Parent process started***\n");
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            exit(EXIT_FAILURE);
        }

        /* opens the input file */
        glob_fd4=input_fd=open(ivalue, O_RDONLY);
        if(input_fd<0){
            perror("program | main | open file error 5");
            exit(EXIT_FAILURE); 
        }

        /* reads the input file and writes the temp file */
        read_input_file(input_fd,fd,pid);

        /* close the temp file */
        if (close(fd) < 0){ 
            perror("program | main | close file error 6"); 
            exit(EXIT_FAILURE); 
        }

        /* close the input file */
        if (close(input_fd) < 0){ 
            perror("program | main | close file error 7"); 
            exit(EXIT_FAILURE); 
        } 

        /* sends the finish signal */
        kill(pid,SIGUSR1);
        sprintf(print_string,"SIGUSR1 signal sent.\n");
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            exit(EXIT_FAILURE);
        }
        /* waits the child process */
        wait(NULL);

        sprintf(print_string,"***Parent process finished***\n");
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            perror("program | failed in writing STDOUT.\n");
            exit(EXIT_FAILURE);
        }
    }
    else{
        /* error */
        perror("program | main | fork error"); 
        exit(EXIT_FAILURE); 
    }
    
}