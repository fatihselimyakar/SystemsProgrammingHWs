/****************************************************/
/* Author     : Fatih Selim Yakar                   */
/* ID         : 161044054                           */
/* Title      : Systems Programming Midterm Project */
/* Instructor : Erchan Aptoula                      */
/****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <time.h>
#include <stdatomic.h>

#define SOUP 0
#define MAIN_COURSE 1
#define DESERT 2
#define INTER_PROCESS 1

/* Shared memory structure */
typedef struct shared_memory{
    /* Used mutexes */
    sem_t kitchen_mutex;
    sem_t counter_mutex;
    sem_t cook_mutex;
    sem_t student_mutex;

    /* Used semaphores and variables for kitchen */
    sem_t empty_kitchen;
    sem_t full_kitchen;
    atomic_int total_of_kitchen;
    sem_t kitchen_soup;
    sem_t kitchen_main_course;
    sem_t kitchen_desert; 

    /* Used semapgores and variables for counter */
    sem_t empty_counter;
    sem_t full_counter;
    atomic_int total_of_counter;
    sem_t counter_soup;
    sem_t counter_main_course;
    sem_t counter_desert; 

    /* Used semaphore for table */
    sem_t full_table;

    atomic_int is_delete_resources;

} shared_memory;

static struct shared_memory* shared_mem;


/* Prints error in the STDERR */
void print_error(char error_message[]){
    if(write(STDERR_FILENO,error_message,strlen(error_message))<0){
        exit(EXIT_FAILURE);
    }
}

/* Does sem_wait with control */
void s_wait(sem_t *sem){
    if(sem_wait(sem)<0){
        print_error("sem_wait error");
        exit(EXIT_FAILURE);
    }
}

/* Does sem_post with control */
void s_post(sem_t *sem){
    if(sem_post(sem)<0){
        print_error("sem_post error");
        exit(EXIT_FAILURE);
    }
}

/* Returns value of the given semaphore */
int s_getval(sem_t* sem){
    int return_val;
    if(sem_getvalue(sem,&return_val)<0){
        print_error("sem_getvalue error");
        exit(EXIT_FAILURE);
    }
    return return_val;
}

/* Reads with locking */
ssize_t read_lock(int fd, void *buf, size_t count){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    lock.l_type=F_RDLCK;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        print_error("program | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);

    lock.l_type=F_UNLCK;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        print_error("program | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    return ret_val;
}

/* The supplier puts soup in the kitchen */
void supplier_service_soup(){
    char print_string[200];
    s_wait(&shared_mem->empty_kitchen);
    s_wait(&shared_mem->kitchen_mutex);

    s_post(&shared_mem->kitchen_soup);
    sprintf(print_string,"The supplier delivered soup - after delivery: kitchen items P:%d,C:%d,D:%d=%d\n",s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->kitchen_mutex);
    s_post(&shared_mem->full_kitchen);
}

/* The supplier puts main course in the kitchen */
void supplier_service_mc(){
    char print_string[200];
    s_wait(&shared_mem->empty_kitchen);
    s_wait(&shared_mem->kitchen_mutex);

    s_post(&shared_mem->kitchen_main_course);
    sprintf(print_string,"The supplier delivered main course - after delivery: kitchen items P:%d,C:%d,D:%d=%d\n",s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->kitchen_mutex);
    s_post(&shared_mem->full_kitchen);
}

/* The supplier puts desert in the kitchen */
void supplier_service_desert(){
    char print_string[200];
    s_wait(&shared_mem->empty_kitchen);
    s_wait(&shared_mem->kitchen_mutex);

    s_post(&shared_mem->kitchen_desert);
    sprintf(print_string,"The supplier delivered desert - after delivery: kitchen items P:%d,C:%d,D:%d=%d\n",s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->kitchen_mutex);
    s_post(&shared_mem->full_kitchen);
}

/* The cook gets soup in the kitchen */
void cook_get_soup(int index){
    char print_string[200];
    s_wait(&shared_mem->full_kitchen);
    s_wait(&shared_mem->kitchen_mutex);

    s_wait(&shared_mem->kitchen_soup);
    ++shared_mem->total_of_kitchen;
    sprintf(print_string,"Cook %d is going to the counter to deliver soup - counter items P:%d,C:%d,D:%d=%d\n",index,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_desert)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_soup));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->kitchen_mutex);
    s_post(&shared_mem->empty_kitchen);
}

/* The cook gets main course in the kitchen */
void cook_get_mc(int index){
    char print_string[200];
    s_wait(&shared_mem->full_kitchen);
    s_wait(&shared_mem->kitchen_mutex);

    s_wait(&shared_mem->kitchen_main_course);
    ++shared_mem->total_of_kitchen;
    sprintf(print_string,"Cook %d is going to the counter to deliver main course - counter items P:%d,C:%d,D:%d=%d\n",index,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_desert)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_soup));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->kitchen_mutex);
    s_post(&shared_mem->empty_kitchen);
}

/* The cook gets desert in the kitchen */
void cook_get_desert(int index){
    char print_string[200];
    s_wait(&shared_mem->full_kitchen);
    s_wait(&shared_mem->kitchen_mutex);

    s_wait(&shared_mem->kitchen_desert);
    ++shared_mem->total_of_kitchen;
    sprintf(print_string,"Cook %d is going to the counter to deliver desert - counter items P:%d,C:%d,D:%d=%d\n",index,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_desert)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_soup));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->kitchen_mutex);
    s_post(&shared_mem->empty_kitchen);
}

/* The cook puts soup in the counter */
void cook_give_soup(int index){
    char print_string[200];
    s_wait(&shared_mem->empty_counter);
    s_wait(&shared_mem->counter_mutex);

    s_post(&shared_mem->counter_soup);
    sprintf(print_string,"Cook %d placed soup on the counter - counter items P:%d,C:%d,D:%d=%d\n",index,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_desert)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_soup));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->counter_mutex);
    s_post(&shared_mem->full_counter);
}

/* The cook puts main course in the counter */
void cook_give_mc(int index){
    char print_string[200];
    s_wait(&shared_mem->empty_counter);
    s_wait(&shared_mem->counter_mutex);

    s_post(&shared_mem->counter_main_course);
    sprintf(print_string,"Cook %d placed main course on the counter - counter items P:%d,C:%d,D:%d=%d\n",index,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_desert)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_soup));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    s_post(&shared_mem->counter_mutex);
    s_post(&shared_mem->full_counter);
}

/* The cook puts desert in the counter */
void cook_give_desert(int index){
    char print_string[200];
    s_wait(&shared_mem->empty_counter);
    s_wait(&shared_mem->counter_mutex);

    s_post(&shared_mem->counter_desert);
    sprintf(print_string,"Cook %d placed desert on the counter - counter items P:%d,C:%d,D:%d=%d\n",index,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_desert)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_soup));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }
   
    s_post(&shared_mem->counter_mutex);
    s_post(&shared_mem->full_counter);
}

/* The student eat a soup */
void student_eat_soup(int i,int round){
    char print_string[200];
    s_wait(&shared_mem->full_counter);
    s_wait(&shared_mem->counter_mutex);

    s_wait(&shared_mem->counter_soup);
    ++shared_mem->total_of_counter;

    s_post(&shared_mem->counter_mutex);
    s_post(&shared_mem->empty_counter);
}

/* The student eat a main course */
void student_eat_mc(int i,int round){
    s_wait(&shared_mem->full_counter);
    s_wait(&shared_mem->counter_mutex);

    s_wait(&shared_mem->counter_main_course);
    ++shared_mem->total_of_counter;

    s_post(&shared_mem->counter_mutex);
    s_post(&shared_mem->empty_counter);
}

/* The student eat a desert */
void student_eat_desert(int i,int round){
    s_wait(&shared_mem->full_counter);
    s_wait(&shared_mem->counter_mutex);

    s_wait(&shared_mem->counter_desert);
    ++shared_mem->total_of_counter;

    s_post(&shared_mem->counter_mutex);
    s_post(&shared_mem->empty_counter);
}


/* The supplier process'es function */
/* Reads the given file one by one and puts the kitchen whatever it reads by using above the supplier functions*/ 
void supplier(int input_fd,char input_file_name[],int l,int m){
    int i;
    char word[1];
    char print_string[200];
    for(i=0;i<3*l*m;++i){
        /* Reads the file one by one */
        if(read_lock(input_fd,word,1)<=0){
            print_error("In supplier read error.");
            exit(EXIT_FAILURE);
        }
        /* If the read food is soup */
        if(word[0]=='P'){
            sprintf(print_string,"The supplier is going to the kitchen to deliver soup: kitchen items P:%d,C:%d,D:%d=%d\n",s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            supplier_service_soup();
        }
        /* If the read food is main course */
        if(word[0]=='C'){
            sprintf(print_string,"The supplier is going to the kitchen to deliver main course: kitchen items P:%d,C:%d,D:%d=%d\n",s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            supplier_service_mc();
        }
        /* If the read food is desert */
        if(word[0]=='D'){
            sprintf(print_string,"The supplier is going to the kitchen to deliver desert: kitchen items P:%d,C:%d,D:%d=%d\n",s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            supplier_service_desert();
        }
        /* Input file closing and termination before the finish */
        if(i==3*l*m-1){
            if(close(input_fd)<0){
                print_error("program | main | close error.");
                exit(EXIT_FAILURE);
            }

            /*if(unlink(input_file_name)<0){
                print_error("program | main | unlink error.");
                exit(EXIT_FAILURE);
            }*/
        }

    }

    /* Supplier's finish */
    sprintf(print_string,"The supplier finished supplying - GOODBYE!\n");
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }
}

/* The cook process'es function */
/* Cook gets the 3*l*m in the kitchen and suddenly transport these foods from kitchen to counter*/
/* Cook firstly takes soup and transports, secondly takes main course and transports, thirdly takes desert and transports and continues this way*/
void cook(int i,int l,int m){
    char print_string[200];
    while (shared_mem->total_of_kitchen<3*l*m)
    {
        /* If there is soup, Also this turn is soup's turn.*/
        s_wait(&shared_mem->cook_mutex);
        if(shared_mem->total_of_kitchen%3==SOUP&&s_getval(&shared_mem->kitchen_soup)>0){
            /* Gets one soup in the kitchen */
            sprintf(print_string,"Cook %d is going to the kitchen to s_wait for/get a plate - kitchen items: P:%d,C:%d,D:%d=%d\n",i,s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            cook_get_soup(i);

            /* Gives one soup in the counter */
            sprintf(print_string,"Cook %d is going to counter to deliver soup - counter items P:%d,C:%d,D:%d=%d\n",i,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_soup)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            cook_give_soup(i);
        }
        s_post(&shared_mem->cook_mutex);

        /* If there is main course, Also this turn is main course's turn.*/
        s_wait(&shared_mem->cook_mutex);
        if(shared_mem->total_of_kitchen%3==MAIN_COURSE&&s_getval(&shared_mem->kitchen_main_course)>0){
            /* Gets one main course in the kitchen */
            sprintf(print_string,"Cook %d is going to the kitchen to s_wait for/get a plate - kitchen items: P:%d,C:%d,D:%d=%d\n",i,s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            cook_get_mc(i);

            /* Gives one main course in the counter */
            sprintf(print_string,"Cook %d is going to counter to deliver main course - counter items P:%d,C:%d,D:%d=%d\n",i,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_soup)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            cook_give_mc(i);
        }
        s_post(&shared_mem->cook_mutex);

        /* If there is desert, Also this turn is desert's turn.*/
        s_wait(&shared_mem->cook_mutex);
        if(shared_mem->total_of_kitchen%3==DESERT&&s_getval(&shared_mem->kitchen_desert)>0){
            /* Gets one desert in the kitchen */
            sprintf(print_string,"Cook %d is going to the kitchen to s_wait for/get a plate - kitchen items: P:%d,C:%d,D:%d=%d\n",i,s_getval(&shared_mem->kitchen_soup),s_getval(&shared_mem->kitchen_main_course),s_getval(&shared_mem->kitchen_desert),s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            cook_get_desert(i);

            /* Gives one desert in the counter */
            sprintf(print_string,"Cook %d is going to counter to deliver desert - counter items P:%d,C:%d,D:%d=%d\n",i,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_soup)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            cook_give_desert(i);
        }
        s_post(&shared_mem->cook_mutex);

    }
    /* Cook's finish */
    sprintf(print_string,"Cook %d finished serving - items at kitchen: %d - going home - GOODBYE!!!\n",i,s_getval(&shared_mem->kitchen_soup)+s_getval(&shared_mem->kitchen_main_course)+s_getval(&shared_mem->kitchen_desert));
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }    
}

/* The student process'es function */
/* The student gets the 3 plates in counter and eats foods in table. Also repeats those L times */
void student(int i,int l,int m,int t){
    int round=0;
    char print_string[200];
    while (shared_mem->total_of_counter<3*l*m)
    {
       
        if(s_getval(&shared_mem->counter_soup)>0 && s_getval(&shared_mem->counter_main_course)>0 && s_getval(&shared_mem->counter_desert)>0){

            sprintf(print_string,"Student %d is going to the counter (round %d) - # of students at counter: %d and counter items P:%d,C:%d,D:%d=%d\n",i,round+1,1,s_getval(&shared_mem->counter_soup),s_getval(&shared_mem->counter_main_course),s_getval(&shared_mem->counter_desert),s_getval(&shared_mem->counter_soup)+s_getval(&shared_mem->counter_main_course)+s_getval(&shared_mem->counter_desert));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }

            s_wait(&shared_mem->student_mutex);

            /* Student gets 3 plates food */
            student_eat_soup(i,round);
            student_eat_mc(i,round);
            student_eat_desert(i,round);

            s_post(&shared_mem->student_mutex);

            sprintf(print_string,"Student %d got food and is going to get a table (round %d) - # of empty tables:%d\n",i,round+1,s_getval(&shared_mem->full_table));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }

            /* Student sits a table and eats */
            s_wait(&shared_mem->full_table);
            int chosen_table=t-s_getval(&shared_mem->full_table);
            sprintf(print_string,"Student %d sat at table %d to eat (round %d) - empty tables:%d\n",i,chosen_table,round+1,s_getval(&shared_mem->full_table));
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }

            ++round;
            /* Student lefts a table and repeats */
            s_post(&shared_mem->full_table);
            if(round!=l){
                 sprintf(print_string,"Student %d left table %d to eat again (round %d) - empty tables:%d\n",i,chosen_table,round+1,s_getval(&shared_mem->full_table));
                if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                    print_error("program | write error");
                    exit(EXIT_FAILURE);
                }   
            }  
        }
       
        /* if round is equal to L then finishes */
        if(round==l){
            sprintf(print_string,"Student %d is done eating L=%d times - going home - GOODBYE!!!\n",i,round);
            if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
                print_error("program | write error");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
}

/* When the SIGINT signal is caught,it will free up all resources an exits */
void sigint_handler(int signum){
    char print_string[50];
    sprintf(print_string,"SIGINT signal caught.Exiting..\n");
    if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
        print_error("program | write error");
        exit(EXIT_FAILURE);
    }

    /* Destroys semaphores */
    if(shared_mem->is_delete_resources!=1){
        shared_mem->is_delete_resources=1;
        if(
            sem_destroy(&shared_mem->kitchen_mutex)<0 ||
            sem_destroy(&shared_mem->cook_mutex)<0    ||
            sem_destroy(&shared_mem->counter_mutex)<0 ||
            sem_destroy(&shared_mem->student_mutex)<0 ||

            sem_destroy(&shared_mem->empty_kitchen)<0 ||
            sem_destroy(&shared_mem->full_kitchen)<0  ||
            sem_destroy(&shared_mem->kitchen_soup)<0  ||
            sem_destroy(&shared_mem->kitchen_desert)<0||
            sem_destroy(&shared_mem->kitchen_main_course)<0 ||
            
            sem_destroy(&shared_mem->empty_counter)<0 ||
            sem_destroy(&shared_mem->full_counter)<0  ||
            sem_destroy(&shared_mem->counter_soup)<0  ||
            sem_destroy(&shared_mem->counter_desert)<0||
            sem_destroy(&shared_mem->counter_main_course)<0 ||
            
            sem_destroy(&shared_mem->full_table)   
        ){
            print_error("program | main | semaphore destroy error.");
            exit(EXIT_FAILURE);
        }

        /* Uninitialize the map */
        if(munmap(shared_mem,sizeof(shared_memory))<0){
            print_error("program | main | munmap error.");
            exit(EXIT_FAILURE);
        }

        /* Unlinks the shared memory */
        if(shm_unlink("shared_mem")<0){
            print_error("program | main | shm_unlink error.");
            exit(EXIT_FAILURE);
        }
        
    }
    exit(EXIT_SUCCESS);
}

/* Firstly, controls the command line arguments and initialize the all resources with these */
/* Secondly, creates the Supplier, Cook and Student process'es by using given parameters */
/* Thirdly, frees resources and exits */
int main(int argc,char *argv[]){
    int opt;
    int n,m,t,s,l,k,fd,input_fd;
    char input_file_name[20];

    struct sigaction sact;
    sigemptyset(&sact.sa_mask); 
    sact.sa_flags = SA_RESTART;
    sact.sa_handler = sigint_handler;

    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &sact, NULL) != 0){
        print_error("SIGINT sigaction() error");
        exit(EXIT_SUCCESS);
    }

    while((opt = getopt(argc, argv, ":N:M:T:S:L:F:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'N':
                n=atoi(optarg);
                if(n<=2){
                    errno=EINVAL;
                    print_error("program | main | N must be greater than 2.");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'M':
                m=atoi(optarg);
                if(m<=n){
                    errno=EINVAL;
                    print_error("program | main | M must be greater than N.");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'S':
                s=atoi(optarg);
                if(s<=3){
                    errno=EINVAL;
                    print_error("program | main | S must be greater than 3.");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'T':
                t=atoi(optarg);
                if(t<1 || t>=m){
                    errno=EINVAL;
                    print_error("program | main | T must be greater or equal than 1 or lower than M.");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'L':
                l=atoi(optarg);
                if(l<3){
                    errno=EINVAL;
                    print_error("program | main | L must be greater or equal than 3.");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'F':
                strcpy(input_file_name,optarg);
                break;
            case ':':  
                errno=EINVAL;
                print_error("program | main | You must write like this: \"./program -N num -M num -T num -S num -L num -F inputPath\"");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                print_error("program | main | You must write like this: \"./program -N num -M num -T num -S num -L num -F inputPath\"");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    /* Kitchen size */
    k=2*l*m+1;

    /* Controls are there more or less parameters */
    if(optind!=13){
        errno=EINVAL;
        print_error("program | main | You must write like this: \"./program -N num -M num -T num -S num -L num -F inputPath\"");
        exit(EXIT_FAILURE);
    }

    /* Opens the supplier's file */
    input_fd=open(input_file_name,O_RDONLY);
    if(input_fd<0){
        print_error("program | main | open error.");
        exit(EXIT_FAILURE);
    }

    /* Semaphore created by shared memory object */
    fd=shm_open("shared_mem",O_CREAT | O_RDWR | O_TRUNC,S_IRWXU | S_IRWXO );
    if(fd<0){
        print_error("program | main | shm_open error.");
        exit(EXIT_FAILURE);
    }

    if(ftruncate(fd,sizeof(shared_mem))<0){
        print_error("program | main | ftruncate error.");
        exit(EXIT_FAILURE);
    }

    /* Allocates the shared memory structure */
    shared_mem=(shared_memory*)mmap(NULL,sizeof(shared_mem),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if((void*)shared_mem==MAP_FAILED){
        print_error("program | main | semaphore mmap error.");
        exit(EXIT_FAILURE);
    }

    /* Initializes the semaphores */
    if( 
        sem_init(&shared_mem->kitchen_mutex,INTER_PROCESS,1)<0 ||
        sem_init(&shared_mem->cook_mutex,INTER_PROCESS,1)<0    ||
        sem_init(&shared_mem->counter_mutex,INTER_PROCESS,1)<0 ||
        sem_init(&shared_mem->student_mutex,INTER_PROCESS,1)<0 ||

        sem_init(&shared_mem->empty_kitchen,INTER_PROCESS,k)<0 ||
        sem_init(&shared_mem->full_kitchen,INTER_PROCESS,0)<0  ||
        sem_init(&shared_mem->kitchen_soup,INTER_PROCESS,0)<0  ||
        sem_init(&shared_mem->kitchen_desert,INTER_PROCESS,0)<0||
        sem_init(&shared_mem->kitchen_main_course,INTER_PROCESS,0)<0 ||
        
        sem_init(&shared_mem->empty_counter,INTER_PROCESS,s)<0 ||
        sem_init(&shared_mem->full_counter,INTER_PROCESS,0)<0  ||
        sem_init(&shared_mem->counter_soup,INTER_PROCESS,0)<0  ||
        sem_init(&shared_mem->counter_desert,INTER_PROCESS,0)<0||
        sem_init(&shared_mem->counter_main_course,INTER_PROCESS,0)<0 ||
        
        sem_init(&shared_mem->full_table,INTER_PROCESS,t) 
    ){
        print_error("program | main | semaphore initialize error.");
        exit(EXIT_FAILURE);
    }

    /* Initializes the variables */
    shared_mem->total_of_kitchen=0;
    shared_mem->total_of_counter=0;
    shared_mem->is_delete_resources=0;

    /* Supplier process generation */
    switch (fork())
    {
    case -1:
        print_error("program | main | supplier fork error.");
        exit(EXIT_FAILURE);
        break;
    case 0:
        supplier(input_fd,input_file_name,l,m);
        
        exit(EXIT_SUCCESS);
    default:
        break;
    }

    /* Cook process generation */
    int i;
    for(i=0;i<n;++i){
        switch (fork())
        {
        case -1:
            print_error("program | main | cook fork error.");
            exit(EXIT_FAILURE);
            break;
        case 0:
            cook(i,l,m);
            exit(EXIT_SUCCESS);
        default:
            break;
        }
    }
    
    /* Student process generation */
    for(i=0;i<m;++i){
        switch (fork())
        {
        case -1:
            print_error("program | main | student fork error.");
            exit(EXIT_FAILURE);
            break;
        case 0:
            student(i,l,m,t);
            exit(EXIT_SUCCESS);
        default:
            break;
        }
    }

    /* Process waiting */
    int stat;
    for(i=0;i<n+m+1;++i){
        char print_string[50];
        sprintf(print_string,"Child (PID=%d) waited.\n",waitpid(-1,&stat,0));
        if(write(STDOUT_FILENO,print_string,strlen(print_string))<0){
            print_error("program | write error");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Destroyes all semaphores */
    if(
        sem_destroy(&shared_mem->kitchen_mutex)<0 ||
        sem_destroy(&shared_mem->cook_mutex)<0    ||
        sem_destroy(&shared_mem->counter_mutex)<0 ||
        sem_destroy(&shared_mem->student_mutex)<0 ||

        sem_destroy(&shared_mem->empty_kitchen)<0 ||
        sem_destroy(&shared_mem->full_kitchen)<0  ||
        sem_destroy(&shared_mem->kitchen_soup)<0  ||
        sem_destroy(&shared_mem->kitchen_desert)<0||
        sem_destroy(&shared_mem->kitchen_main_course)<0 ||
        
        sem_destroy(&shared_mem->empty_counter)<0 ||
        sem_destroy(&shared_mem->full_counter)<0  ||
        sem_destroy(&shared_mem->counter_soup)<0  ||
        sem_destroy(&shared_mem->counter_desert)<0||
        sem_destroy(&shared_mem->counter_main_course)<0 ||
        
        sem_destroy(&shared_mem->full_table)   
      ){
        print_error("program | main | semaphore destroy error.");
        exit(EXIT_FAILURE);
    }

    /* Uninitialize the shared memory */
    if(munmap(shared_mem,sizeof(shared_memory))<0){
        print_error("program | main | munmap error.");
        exit(EXIT_FAILURE);
    }

    /* Unlinks the shared memory */
    if(shm_unlink("shared_mem")<0){
        print_error("program | main | shm_unlink error.");
        exit(EXIT_FAILURE);
    }

}