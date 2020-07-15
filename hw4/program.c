/****************************************************/
/* Author     : Fatih Selim Yakar                   */
/* ID         : 161044054                           */
/* Title      : Systems Programming HW4             */
/* Instructor : Erchan Aptoula                      */
/****************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MILK 'M'
#define FLOUR 'F'
#define WALNUTS 'W'
#define SUGAR 'S'
#define NONE '0' /* If chef uses the ingredients then puts that after the using */
#define FINISHED 'D' /* If wholesaler gives all of the ingredients then puts that after the job is finished */

#define SEM_MUTEX_KEY "sem-mutex-key" /* System V semaphore key (a file) for sem_mutex */
#define SEM_WHOLESALER_MUTEX_KEY "sem-wholesaler-mutex-key" /* System V semaphore key (a file) for sem_wholesaler_mutex */

/* Buffer data structures */
char *buf; /* Array that using the shared ingredients */
char **chefs_lacks; /* Normally it should be [6][2] but [3] in this case because of the string's end character('\0') */

int mutex_sem; /* It was used to prevent threads from running at the same time */
int wholesaler_mutex_sem; /* It was used to wait until the desert was completed */

/* Prints error in the STDERR */
void print_error(char error_message[]){
    if(write(STDERR_FILENO,error_message,strlen(error_message))<0){
        exit(EXIT_FAILURE);
    }
}

/* Prints string in the STDOUT */
void print_string(char string[]){
    if(write(STDOUT_FILENO,string,strlen(string))<0){
        print_error("program | print_string | write error\n");
        exit(EXIT_FAILURE);
    }
}

/* Reads with locking */
ssize_t read_lock(int fd, void *buf, size_t count){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_RDLCK;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        print_error("program | read_lock | fcntl error\n");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);

    lock.l_type=F_UNLCK;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        print_error("program | read_lock | fcntl error\n");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    return ret_val;
}

/* Chooses the random lack ingredient for chefs */
void choose_random_lack_ingredient(char wanted_ingredients[]){
    int choose=rand()%6;  
    while(1){
        if(strcmp(chefs_lacks[choose],"NN")!=0){
            wanted_ingredients[0]=chefs_lacks[choose][0];
            wanted_ingredients[1]=chefs_lacks[choose][1];
            strcpy(chefs_lacks[choose],"NN");
            return;
        }
        choose=(choose+1)%6;
    }
}

/* Thread function of the chefs */
void *chef (void *arg){
    char will_print[200];
    struct sembuf asem [1]; /* for System V semaphores */
    char wanted_ingredients[2]; /* for using */
    char string_ingredients[2][10]; /* for printing */

    asem [0].sem_num = 0;
    asem [0].sem_op = 0;
    asem [0].sem_flg = 0;

    //wanted_ingredients[0]=chefs_lacks[*((int *)arg)][0];
    //wanted_ingredients[1]=chefs_lacks[*((int *)arg)][1];

    /* Chooses random pair for chef(*arg)'s lackness */
    choose_random_lack_ingredient(wanted_ingredients);
    
    /* All of under these for printing */
    if(wanted_ingredients[0]==MILK){
        strcpy(string_ingredients[0],"milk");
    }
    else if(wanted_ingredients[0]==WALNUTS){
        strcpy(string_ingredients[0],"walnuts");
    }
    else if (wanted_ingredients[0]==SUGAR){
        strcpy(string_ingredients[0],"sugar");   
    }
    else if (wanted_ingredients[0]==FLOUR){
        strcpy(string_ingredients[0],"flour");   
    }

    if(wanted_ingredients[1]==MILK){
        strcpy(string_ingredients[1],"milk");
    }
    else if(wanted_ingredients[1]==WALNUTS){
        strcpy(string_ingredients[1],"walnuts");
    }
    else if (wanted_ingredients[1]==SUGAR){
        strcpy(string_ingredients[1],"sugar");   
    }
    else if (wanted_ingredients[1]==FLOUR){
        strcpy(string_ingredients[1],"flour");   
    }

    sprintf(will_print,"chef%d is waiting for %s and %s\n",*((int *)arg),string_ingredients[0],string_ingredients[1]);
    print_string(will_print);

    while(1){
        if((buf[0]==wanted_ingredients[0] && buf[1]==wanted_ingredients[1]) ||
            (buf[1]==wanted_ingredients[0] && buf[0]==wanted_ingredients[1])){

            /* wait operation on mutex_sem */
            asem [0].sem_op = -1;                    
            if (semop (mutex_sem, asem, 1) == -1) {  
                print_error ("program | chef | semop error\n");  
                exit (EXIT_FAILURE);                 
            }   
            /* Critical region */
            sprintf(will_print,"chef%d has taken the %s\n",*((int *)arg),string_ingredients[0]);
            print_string(will_print);
            buf[0]=NONE; /* Takes the buf[0] ingredient */
            sprintf(will_print,"chef%d has taken the %s\n",*((int *)arg),string_ingredients[1]);
            print_string(will_print);
            buf[1]=NONE; /* Takes the buf[1] ingredient */

            sprintf(will_print,"chef%d is preparing the dessert\n",*((int *)arg));
            print_string(will_print);
            
            /* Sleeps [1,5] seconds */
            sleep((rand()%5)+1);

            /* post operation on mutex_sem */
            asem [0].sem_op = 1;                    
            if (semop (mutex_sem, asem, 1) == -1) {  
                print_error ("program | chef | semop error\n");   
                exit (EXIT_FAILURE);                 
            }

            /* post operation on wholesaler_mutex_sem */
            sprintf(will_print,"chef%d has delivered the dessert to the wholesaler\n",*((int *)arg));
            print_string(will_print);
            asem [0].sem_op = 1;                     
            if (semop (wholesaler_mutex_sem, asem, 1) == -1) {  
                print_error ("program | chef | semop error\n");  
                exit (EXIT_FAILURE);                 
            }
            sprintf(will_print,"chef%d is waiting for %s and %s\n",*((int *)arg),string_ingredients[0],string_ingredients[1]);
            print_string(will_print);

        }
        /* If wholesaler is finished then the thread finishes */
        if(buf[0]==FINISHED && buf[1]==FINISHED){
            break;
        }
           
    }
    return NULL;
}

/* Function for wholesaler */
void wholesaler(int fd){
    char will_print[200];
    char str[3];
    char ingredients[2][10];
    int rv,i=0;
    struct sembuf asem [1];

    asem [0].sem_num = 0;
    asem [0].sem_op = 0;
    asem [0].sem_flg = 0;

    while(1){
        if((rv=read_lock(fd,str,3))<0){
            print_error("program | wholesaler | read lock error\n");
            exit(EXIT_FAILURE);
        }
        /* if read is equal to 0 then break the loop */
        if(rv==0)
            break;
        
        /* wait operation on mutex_sem */
        asem [0].sem_op = -1;                    
        if (semop (mutex_sem, asem, 1) == -1) {  
	        print_error ("program | wholesaler | semop error\n");   
            exit (EXIT_FAILURE);                 
        }                                        
        /* Critical Section */
        buf[0]=str[0]; buf[1]=str[1];
        //("%s\n",buf);
        if(buf[0]==MILK){
            strcpy(ingredients[0],"milk");
        }
        else if(buf[0]==FLOUR){
            strcpy(ingredients[0],"flour");
        }
        else if(buf[0]==WALNUTS){
            strcpy(ingredients[0],"walnuts");
        }
        else if(buf[0]==SUGAR){
            strcpy(ingredients[0],"sugar");
        }

        if(buf[1]==MILK){
            strcpy(ingredients[1],"milk");
        }
        else if(buf[1]==FLOUR){
            strcpy(ingredients[1],"flour");
        }
        else if(buf[1]==WALNUTS){
            strcpy(ingredients[1],"walnuts");
        }
        else if(buf[1]==SUGAR){
            strcpy(ingredients[1],"sugar");
        }
        sprintf(will_print,"The wholesaler delivers %s and %s\n",ingredients[0],ingredients[1]);
        print_string(will_print);
        
        /* post operation on mutex_sem */
        asem [0].sem_op = 1;                     
        if (semop (mutex_sem, asem, 1) == -1) {  
	        print_error ("program | wholesaler | semop error\n");   
            exit (EXIT_FAILURE);                 
        }                                        

        /* wait operation on wholesaler_mutex_sem */
        sprintf(will_print,"the wholesaler is waiting for the dessert\n");
        print_string(will_print);
        asem [0].sem_op = -1;                     
        if (semop (wholesaler_mutex_sem, asem, 1) == -1) {  
	        print_error ("program | wholesaler | semop error\n");   
            exit (EXIT_FAILURE);                 
        }  
        print_string("the wholesaler has obtained the dessert and left to sell it\n");

        ++i;
        //printf("%s",str);
    }
    if(i<10){
        print_error("program | wholesaler | read file is not greater or equal than 10 line\n");
        exit(EXIT_FAILURE);
    }

    buf[0]=FINISHED; 
    buf[1]=FINISHED;
    print_string("WHOLESALER DONE\n");

    return;
}

int main(int argc,char *argv[]){
    int opt;
    int input_fd,sem_fd,sem_fd2;
    char input_file_name[20];
    pthread_t tid_chef [6];
    int i;

    /* Sys V semaphore */
    key_t s_key[2];
    union semun  
    {
        int val;
        struct semid_ds *buf;
        ushort array [1];
    } sem_attr;

    /* for random choose lacks */
    srand(time(NULL));

    /* Buffer's initialization */
    buf=(char*)malloc(2*sizeof(char));

    chefs_lacks=(char **)malloc(6*sizeof(char*));
    for(i=0;i<6;++i){
        chefs_lacks[i]=(char*)malloc(3*sizeof(char));
    }

    /* All of chef's lacks */    
    strcpy(chefs_lacks[0],"SW");
    strcpy(chefs_lacks[1],"MW");
    strcpy(chefs_lacks[2],"MF");
    strcpy(chefs_lacks[3],"FW");
    strcpy(chefs_lacks[4],"MS");
    strcpy(chefs_lacks[5],"FS");

    while((opt = getopt(argc, argv, ":i:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                strcpy(input_file_name,optarg);
                break;
            case ':':  
                errno=EINVAL;
                print_error("program | main | You must write like this: \"./program -i inputPath\"\n");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                print_error("program | main | You must write like this: \"./program -i inputPath\"\n");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    /* Controls are there more or less parameters */
    if(optind!=3){
        errno=EINVAL;
        print_error("program | main | You must write like this: \"./program -i inputPath\"\n");
        exit(EXIT_FAILURE);
    }

    /* Opens the wholesaler's file */
    input_fd=open(input_file_name,O_RDONLY);
    if(input_fd<0){
        print_error("program | main | open file error.\n");
        exit(EXIT_FAILURE);
    }

    /* Opens the files for using Sys V semaphores */
    sem_fd=open(SEM_MUTEX_KEY,O_RDWR | O_CREAT);
    if(sem_fd<0){
        print_error("program | main | open file error.\n");
        exit(EXIT_FAILURE);
    }

    sem_fd2=open(SEM_WHOLESALER_MUTEX_KEY,O_RDWR | O_CREAT);
    if(sem_fd2<0){
        print_error("program | main | open file error.\n");
        exit(EXIT_FAILURE);
    }
    /* Converts a pathname and a project identifier to a System V IPC key */
    if ((s_key[0] = ftok (SEM_MUTEX_KEY, 'a')) == -1 ||
        (s_key[1] = ftok (SEM_WHOLESALER_MUTEX_KEY,'a'))==-1) { 
        print_error ("program | main | ftok error\n"); 
        exit (EXIT_FAILURE);
    }

    /* Creates a semaphores */
    if ((mutex_sem = semget (s_key[0], 1, 0660 | IPC_CREAT)) == -1 ||
        (wholesaler_mutex_sem = semget(s_key[1], 1, 0660 | IPC_CREAT)) == -1) {
        print_error ("program | main | semget error\n"); 
        exit (EXIT_FAILURE);
    }

    /* Giving initial value the mutexes. */ 

    /* mutex_sem initialized by 1 */
    sem_attr.val = 1;     
    if (semctl (mutex_sem, 0, SETVAL, sem_attr) == -1) { 
        print_error ("program | main | semctl SETVAL error\n"); 
        exit (EXIT_FAILURE);
    }

    /* wholesaler_mutex_sem initialized by 0 */
    sem_attr.val = 0;
    if (semctl (wholesaler_mutex_sem, 0, SETVAL, sem_attr) == -1) { 
        print_error ("program | main | semctl SETVAL error\n"); 
        exit (EXIT_FAILURE);
    }


    /* Create 6 chef threads */
    int thread_no [6];
    for (i = 0; i < 6; i++) {
        thread_no [i] = i;
        if (pthread_create (&tid_chef [i], NULL, chef, (void *) &thread_no [i]) != 0) { 
            print_error ("program | main | pthread_create error\n"); 
            exit (EXIT_FAILURE);
        }
    }
	
	/* Take a nap for good prints :) */
    //sleep(1);
    
    /* Wholesaler function */
    wholesaler(input_fd);

    /* Wait(join) for chefs to terminate */
    for (i = 0; i < 6; i++){
        if (pthread_join (tid_chef [i], NULL) == -1) { 
            print_error ("program | main | pthread_join error\n"); 
            exit (EXIT_FAILURE);
        }
    }
        
    /* Closes the used files */
    if(close(sem_fd)<0 || close(sem_fd2)<0 ||close(input_fd)<0){
        print_error("program | main | Close file error\n");
        exit(EXIT_FAILURE);
    }

    /* Unlinks the used semaphore files */
    if(unlink(SEM_WHOLESALER_MUTEX_KEY)<0 || unlink(SEM_MUTEX_KEY)<0){
        print_error("program | main | Unlink file error\n");
        exit(EXIT_FAILURE);
    }

    /* Remove semaphores */
    if (semctl (mutex_sem, 0, IPC_RMID) == -1 || semctl(wholesaler_mutex_sem,0,IPC_RMID) == -1) {
        print_error("program | main | semctl IPC_RMID error\n");
        exit(EXIT_FAILURE);
    }

    /* Freeing the resources */
    free(buf);
    for(i=0;i<6;++i){
        free(chefs_lacks[i]);
    }
    free(chefs_lacks);

    exit(EXIT_SUCCESS);
}