/****************************************************/
/* Author     : Fatih Selim Yakar                   */
/* ID         : 161044054                           */
/* Title      : Systems Programming HW5             */
/* Instructor : Erchan Aptoula                      */
/****************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <stdatomic.h>

/* Buffer size that holds the 1 line */
#define BUFFERSIZE (64 * 1024)
/* Macro that calculates the chebyshev distance */
#define CHEBYSHEV_DISTANCE(X1,Y1,X2,Y2) ((fabs(X1-X2)>fabs(Y1-Y2) ? fabs(X1-X2) : fabs(Y1-Y2)))

/* Client struct that holds the client's informations */
struct Client 
{ 
   int client_num;
   double coord_X;
   double coord_Y;
   char flower[50];
};

/* Florist struct that holds the florist's informations */
struct Florist 
{ 
   char name[50];
   double coord_X;
   double coord_Y;
   double speed;
   char **flower_list;
   int flower_size;

   struct Client *queue;
   atomic_int queue_size;

   int total_ms_time;
   atomic_int number_of_sales;

};

/* Florist array */
struct Florist* florists;
atomic_int florists_size;

/* Client array */
struct Client* clients;
atomic_int clients_size;

/* Florist and main thread's mutex and condition variables */
pthread_mutex_t* mutexes;
pthread_cond_t* conditions;

/* holds delivered clients */
atomic_int delivered_clients;

/* Other global variables */
int input_fd;
int* thread_no;
pthread_t* tid_florist;
sigset_t pending;

/* Prints error in the STDERR */
void print_error(char error_message[]){
    if(write(STDERR_FILENO,error_message,strlen(error_message))<0){
        exit(EXIT_FAILURE);
    }
}

/* Prints string in the STDOUT */
void print_string(char string[]){
    if(write(STDOUT_FILENO,string,strlen(string))<0){
        print_error("floristApp | print_string | write error\n");
        exit(EXIT_FAILURE);
    }
}

/* Reads with locking */
ssize_t read_lock(int fd, void *buf, size_t count){
    struct flock lock;
    memset(&lock,0,sizeof(lock));
    ssize_t ret_val;

    /* Locks the file */
    lock.l_type=F_RDLCK;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        print_error("floristApp | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }
    
    ret_val=read(fd,buf,count);
    
    /* Unlocks the file */
    lock.l_type=F_UNLCK;
    if(fcntl(fd,F_SETLKW,&lock)<0){
        print_error("floristApp | read_lock | fcntl error");
        close(fd); 
        exit(EXIT_FAILURE);
    }

    return ret_val;

}

/* Reads the 1 line after the offset. */
/* Does not include end \n to the output string */
ssize_t read_line (char *buf, size_t size, int fd, off_t *offset){
    ssize_t nchr = 0;
    ssize_t index = 0;
    char *p = NULL;

    if ((nchr=lseek (fd, *offset, SEEK_SET))!=-1)
        nchr=read_lock(fd, buf, size);
    if (nchr == -1) {
        print_error("floristApp | read_line | read error");
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
        return nchr < (ssize_t)size ? nchr : 0;
    }

    *offset += index + 1;

    return index;
}

/* Read the input file then fills global variables and global arrays */
void read_file_and_save_array(int fd){
    off_t offset=0;
    char line[BUFFERSIZE];
    int len=0,i=0,client_ct=0,florist_ct=0,client_limit=0,florist_limit=0;
    do{
        len=read_line (line, BUFFERSIZE, fd, &offset);
        if(strcmp(line,"")!=0){
            /* if this line is client */
            if(strncmp(line,"client",6)==0){
                char* token = strtok(line, " ();:,\n");
                int token_ct=0; 
                if(client_ct==0){
                    clients=(struct Client*)malloc(sizeof(struct Client)*1000);
                    client_limit=1000;
                }
                else if(client_ct==client_limit-1){
                    clients=(struct Client*)realloc(clients,sizeof(struct Client)*(client_limit+1000));
                    client_limit+=1000;

                }
                /* Seperates the tokens and initializes the global struct arrays */
                while (token != NULL) {
                    /* Client num initialization */
                    if(token_ct==0){
                        clients[client_ct].client_num=client_ct+1;
                        clients_size=client_ct+1;
                    }
                    /* Client x coordinate initialization */
                    else if (token_ct==1){
                        clients[client_ct].coord_X=atof(token);
                    }
                    /* Client y coordinate initialization */
                    else if (token_ct==2){
                        clients[client_ct].coord_Y=atof(token);
                    }
                    /* Client flower initialization */
                    else if (token_ct==3){
                        strcpy(clients[client_ct].flower,token);
                    }
                    token = strtok(NULL, " ();:,\n"); 
                    ++token_ct;
                }
                ++client_ct;
            }
            /* if this line is florist */
            else{
                char* token = strtok(line, " ();:,\n");
                int token_ct=0; 
                if(florist_ct==0){
                    florists=(struct Florist*)malloc(sizeof(struct Florist)*10);
                    florist_limit=10;
                }
                else if(florist_ct==florist_limit-1){
                    florists=(struct Florist*)realloc(florists,sizeof(struct Florist)*(florist_limit+10));
                    florist_limit+=10;
                }
                /* Seperates the tokens and initializes the global struct arrays */
                while (token != NULL) {  
                    /* Florist name initialization */
                    if(token_ct==0){
                        strcpy(florists[florist_ct].name,token);
                        florists_size=florist_ct+1;
                    }
                    /* Florist x coordinate initialization */
                    else if (token_ct==1){
                        florists[florist_ct].coord_X=atof(token);
                    }
                    /* Florist y coordinate initialization */
                    else if (token_ct==2){
                        florists[florist_ct].coord_Y=atof(token);
                    }
                    /* Florist speed initialization */
                    else if (token_ct==3){
                        if((florists[florist_ct].speed=atof(token))<=0){
                            print_error("Speed parameter must be greater than zero !!\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                    /* Florist flower array initialization */
                    else if (token_ct>=4){
                        if(token_ct==4){
                            florists[florist_ct].flower_list=(char **)malloc(sizeof(char*)*(token_ct-3));
                            florists[florist_ct].flower_size=token_ct-3;
                        }
                        else if(token_ct>4){
                            florists[florist_ct].flower_list=(char **)realloc(florists[florist_ct].flower_list,sizeof(char*)*(token_ct-3));
                            florists[florist_ct].flower_size=token_ct-3;
                        }
                        
                        florists[florist_ct].flower_list[token_ct-4]=(char*)malloc(strlen(token)*sizeof(char));
                        strncpy(florists[florist_ct].flower_list[token_ct-4],token,strlen(token));
                    }
                    token = strtok(NULL, " ();:,\n"); 
                    ++token_ct;
                }
                ++florist_ct;
            }
        }
        ++i;
        strcpy(line,"");
    }
    while (len!= -1);

    /* florist's queue initialization */
    for(i=0;i<florists_size;++i){
        florists[i].queue=(struct Client*)malloc(sizeof(struct Client)*clients_size);
        florists[i].queue_size=0;
    }
    /* Mutex and pthread item's initialization */
    tid_florist=(pthread_t*)malloc(florists_size*sizeof(pthread_t));

    thread_no=(int*)malloc(sizeof(int)*florists_size);

    mutexes=(pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*florists_size);

    conditions=(pthread_cond_t*)malloc(sizeof(pthread_cond_t)*florists_size);

    delivered_clients=0;
}

/* Controls the given parameter florist includes the given parameter flower name */
/* if there is then returns 1, otherwise returns 0 */
int is_there_flower(char* flower_name,int florist_index){
    int i;
    for(i=0;i<florists[florist_index].flower_size;++i){
        if(strncmp(florists[florist_index].flower_list[i],flower_name,strlen(flower_name))==0){
            return 1;
        }
    }
    return 0;
}

/* Returns the closest florist's index of given (x,y) coordinate */
int find_closest_florist(struct Client client){
    int i,min_index=-1;
    double min_value=999999;
    for(i=0;i<florists_size;++i){
        double distance=CHEBYSHEV_DISTANCE(client.coord_X,client.coord_Y,florists[i].coord_X,florists[i].coord_Y);
        if(distance<min_value && is_there_flower(client.flower,i)){
            min_value=distance;
            min_index=i;
        }
    }
    return min_index;
}

/* It is the function that performs the task of the main thread. According to the client list in the global array, it finds the nearest florist with mutex and condition variables and adds this client to its queue. */
void central_thread(){
    int i,closest_florist=-1,failed_client=0;
    print_string("Processing requests\n");

    /* Distributes clients to florist's queues */
    for(i=0;i<clients_size;++i){
        closest_florist=find_closest_florist(clients[i]);
        if(closest_florist!=-1){
            pthread_mutex_lock(&mutexes[closest_florist]);

            /* Adds clients to queue */
            florists[closest_florist].queue[florists[closest_florist].queue_size]=clients[i];
            ++florists[closest_florist].queue_size;
            pthread_cond_signal(&conditions[closest_florist]);

            pthread_mutex_unlock(&mutexes[closest_florist]);
        }
        else{
            ++failed_client;
        }
    }
    /* to extract failed client to total client size */
    clients_size-=failed_client;
}

/* Provides millisecond sleep */
int msleep(unsigned int time) {
  return usleep(time * 1000);
}

/* It is the function that does the work of florists. The florist delivers flowers after sleeping, according to the client he wrote in the main thread in queue. */
void *florist (void *arg){
    int florist_index=*((int *)arg);
    int times_in_sleep=0,total_time=0;
    florists[florist_index].number_of_sales=0;
        
    while(1){
	    char will_print[500];
        if(sigpending(&pending) <0) {
            print_error("floristApp | florist | sigpending error\n");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&mutexes[florist_index]);
        while(florists[florist_index].queue_size==0 && delivered_clients !=clients_size && !sigismember(&pending, SIGINT)){
            pthread_cond_wait(&conditions[florist_index],&mutexes[florist_index]);
        }

        /* Finishing condition */
        if((delivered_clients==clients_size || sigismember(&pending, SIGINT)) && florists_size>1){
            int i;
            pthread_mutex_unlock(&mutexes[florist_index]);
            sprintf(will_print,"%s closing shop.\n",florists[florist_index].name);
            print_string(will_print);
            /* Signals other threads to wake up */
            for(i=0;i<florists_size;++i){
                if(pthread_cond_signal(&conditions[i])!=0){
                    print_error("floristApp | central_thread |pthread_cond_signal error.\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;
        }

        /* Gets client and delivers */
        --florists[florist_index].queue_size;
        ++florists[florist_index].number_of_sales;
        times_in_sleep=((rand()%250)+1)+CHEBYSHEV_DISTANCE(florists[florist_index].coord_X,florists[florist_index].coord_Y,florists[florist_index].queue[florists[florist_index].queue_size].coord_X,florists[florist_index].queue[florists[florist_index].queue_size].coord_Y)/florists[florist_index].speed;
        total_time+=times_in_sleep;
        msleep(times_in_sleep);
        ++delivered_clients; 

        sprintf(will_print,"Florist %s has delivered a %s to client%d in %dms\n",florists[florist_index].name,florists[florist_index].queue[florists[florist_index].queue_size].flower,florists[florist_index].queue[florists[florist_index].queue_size].client_num,times_in_sleep);
        print_string(will_print);
        pthread_mutex_unlock(&mutexes[florist_index]);

        /* Finishing condition */
        if(delivered_clients==clients_size || sigismember(&pending, SIGINT)){
            int i;
            if(delivered_clients==clients_size)
                print_string("All requests processed.\n");
            sprintf(will_print,"%s closing shop.\n",florists[florist_index].name);
            print_string(will_print);
            /* Signals other threads to wake up */
            for(i=0;i<florists_size;++i){
                if(pthread_cond_signal(&conditions[i])!=0){
                    print_error("floristApp | central_thread |pthread_cond_signal error.\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;
        }          
    }
    
    florists[florist_index].total_ms_time=total_time;
    
    return (void*)&florists[florist_index];
}

/* SIGINT handler function */
void sigint_handler(int signum){
    int i,j;
    char will_print[500];
    sprintf(will_print,"Signal SIGINT(%d) caught, exiting..\n",signum);
    print_string(will_print);

    /* File closing */
    if(close(input_fd)<0){
        print_error("floristApp | sigint_handler | Close file error\n");
        exit(EXIT_FAILURE);
    }
    print_string("File closed\n");

    /* Destroys the mutexes */
    for(i=0;i<florists_size;++i){
        if(pthread_mutex_destroy(&mutexes[i])!=0){
			pthread_mutex_unlock(&mutexes[i]);
			if(pthread_mutex_destroy(&mutexes[i])!=0){
				print_error("floristApp | main | Mutex destroy error\n");
            	exit(EXIT_FAILURE);
			}
        }
    }
    
    /* Thread items destroys */
    for(i=0;i<florists_size;++i){
        if(pthread_cond_destroy(&conditions[i])!=0){
            print_error("floristApp | sigint_handler | Condition destroy error\n");
            exit(EXIT_FAILURE);
        }
    }
    print_string("Thread items destroyed\n");

    /* Frees other resources */
    for(i=0;i<florists_size;++i){
        for(j=0;j<florists[i].flower_size;++j){
             free(florists[i].flower_list[j]);
        }
        free(florists[i].flower_list);
        free(florists[i].queue);
    }
    free(tid_florist);
    free(thread_no);
    free(florists);
    free(clients);
    free(mutexes);
    free(conditions);
    print_string("Variables freed\n");

    exit(EXIT_SUCCESS);
}

/* Main function */
int main(int argc,char *argv[]){
    int opt,i,j;
    char input_file_name[500],will_print[500];
    void* output;
    sigset_t mask;

    struct sigaction sact;
    sigemptyset(&sact.sa_mask); 
    sact.sa_flags = 0;
    sact.sa_handler = sigint_handler;

    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &sact, NULL) != 0){
        print_error("floristApp| main | SIGINT sigaction() error\n");
        exit(EXIT_FAILURE);
    }
    
    /* Blocks the SIGINT until threads ending */
    sigemptyset(&mask);
    sigaddset(&mask,SIGINT);
    if(sigprocmask(SIG_BLOCK, &mask, 0) < 0) {
        print_error("floristApp| main | sigprocmask block error\n");
        exit(EXIT_FAILURE);
    }

    while((opt = getopt(argc, argv, ":i:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                strcpy(input_file_name,optarg);
                break;
            case ':':  
                errno=EINVAL;
                print_error("floristApp | main | You must write like this: \"./floristApp -i inputPath\"\n");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                print_error("floristApp | main | You must write like this: \"./floristApp -i inputPath\"\n");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    /* Controls are there more or less parameters */
    if(optind!=3){
        errno=EINVAL;
        print_error("floristApp | main | You must write like this: \"./floristApp -i inputPath\"\n");
        exit(EXIT_FAILURE);
    }

    sprintf(will_print,"Florist application initializing from file: %s\n",input_file_name);
    print_string(will_print);

    srand(time(NULL));

    /* Opens the data file */
    input_fd=open(input_file_name,O_RDONLY);
    if(input_fd<0){
        print_error("floristApp | main | open file error.\n");
        exit(EXIT_FAILURE);
    }

    /* Reads file and initializes the global variables and arrays */
    read_file_and_save_array(input_fd);

    /* Mutex initializations */
    for(i=0;i<florists_size;++i){
        if(pthread_mutex_init(&mutexes[i],NULL)!=0){
            print_error("floristApp | main | Mutex initialize error\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Condition variable initialization */
    for(i=0;i<florists_size;++i){
        if(pthread_cond_init(&conditions[i],NULL)!=0){
            print_error("floristApp | main | Condition initialize error\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Creates florist threads */
    for (i = 0; i < florists_size; ++i) {
        thread_no [i] = i;
        if (pthread_create (&tid_florist [i], NULL, florist, (void *) &thread_no [i]) != 0) { 
            print_error ("floristApp | main | pthread_create error\n"); 
            exit (EXIT_FAILURE);
        }
    }

    sprintf(will_print,"%d florists have been created\n",florists_size);
    print_string(will_print);

    /* Central thread function */
    central_thread();

    /* Wait(join) for florists to terminate */
    struct Florist output_array[florists_size];
    for (i = 0; i < florists_size; ++i){
        if (pthread_join (tid_florist [i], &output) == -1) { 
            print_error ("floristApp | main | pthread_join error\n"); 
            exit (EXIT_FAILURE);
        }
        output_array[i]=*((struct Florist*)output);
    }

    /* Unblocks the SIGINT */
    if(sigprocmask(SIG_UNBLOCK,&mask,0) < 0) {
        print_error("floristApp | main | sigprocmask unblock error\n");
        exit(EXIT_FAILURE);
    }

    /* Prints sale statistics */
    print_string("Sale statistics for today:\n");
    print_string("--------------------------------------------------------------\n");
    print_string(" Florist                 # of sales               Total time\n");
    print_string("--------------------------------------------------------------\n");
    for(i=0;i<florists_size;++i){
        sprintf(will_print,"%7s                  %6d                    %5dms\n",output_array[i].name,output_array[i].number_of_sales,output_array[i].total_ms_time);
        print_string(will_print);
    }
    print_string("--------------------------------------------------------------\n");

    /* Closes the used files */
    if(close(input_fd)<0){
        print_error("floristApp | main | Close file error\n");
        exit(EXIT_FAILURE);
    }

    /* Destroys the mutexes */
    for(i=0;i<florists_size;++i){
        if(pthread_mutex_destroy(&mutexes[i])!=0){
			pthread_mutex_unlock(&mutexes[i]);
			if(pthread_mutex_destroy(&mutexes[i])!=0){
				print_error("floristApp | main | Mutex destroy error\n");
            	exit(EXIT_FAILURE);
			}
        }
    }
    
    /* Destroys the condition variable */
    for(i=0;i<florists_size;++i){
        if(pthread_cond_destroy(&conditions[i])!=0){
            print_error("floristApp | main | Condition destroy error\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Frees the all resources */
    for(i=0;i<florists_size;++i){
        for(j=0;j<florists[i].flower_size;++j){
             free(florists[i].flower_list[j]);
        }
        free(florists[i].flower_list);
        free(florists[i].queue);
    }

    free(tid_florist);
    free(thread_no);
    free(florists);
    free(clients);
    free(mutexes);
    free(conditions);

    return 0;
}

