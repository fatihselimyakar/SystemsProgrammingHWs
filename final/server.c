/****************************************************/
/* Author     : Fatih Selim Yakar                   */
/* ID         : 161044054                           */
/* Title      : Systems Programming Final - Server  */
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
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <ctype.h>
#include <stdatomic.h>
#include <netdb.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

#include "input_output.h"
#include "queue.h"
#include "graph.h"
#include "cache.h"

/* Graph and database cache */
struct Graph* graph;
struct Cache* cache;

/* An inter-thread data structure that holds socket file descriptors */
struct Queue** socketfd_queues;

/* Log file's file descriptor */
int output_fd;

/*  Threads's mutex and condition variables */
pthread_mutex_t* mutexes;
pthread_cond_t* conditions_empty;

/* Thread's variables */
int* thread_no;
pthread_t* tid_threads;
atomic_int running_thread_size;
atomic_int total_thread_size;

/* Dynamic pool control thread's variables */
pthread_t pool_thread_id;
pthread_mutex_t pool_mutex;
pthread_cond_t pool_cond;

/* Socket's variables */
socklen_t len; 
struct sockaddr_in cli;
int sockfd;

/* Readers and writer design's variables */
atomic_int active_readers;
atomic_int active_writers;
atomic_int waiting_readers;
atomic_int waiting_writers;
pthread_cond_t ok_to_read;
pthread_cond_t ok_to_write;
pthread_mutex_t rw_mutex;

/* Sigint handler flag */
atomic_int sigint_flag;

int rw_priority;

/* Transforms the current process to daemon */
static void create_daemon()
{
    pid_t pid;

    /* Forks the parent process */
    if ((pid = fork())<0){
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Terminates the parent process */
    if (pid > 0){
        exit(EXIT_SUCCESS);
    }

    /*The forked process is session leader */
    if (setsid() < 0){
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Second fork */
    if((pid = fork())<0){
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Parent termination */
    if (pid > 0){
        exit(EXIT_SUCCESS);
    }

    /* Unmasks */
    umask(0);

    /* Appropriated directory changing */
    chdir(".");

    /* Close core  */
    close(STDERR_FILENO);
    close(STDOUT_FILENO);
    close(STDIN_FILENO);

}

/* Function that takes the path request from the client and calculates the path according to the request. Applies the Writers-readers thread structure. */
void get_request(int sockfd,int thread_index) 
{ 
    char buf[200],*cache_path,will_print[1500000]; 
    int first_node,end_node;
    char* will_send=calloc(999999,sizeof(char));

    bzero(buf, 200); 
    /* Reads the socket */
    read(sockfd, buf, sizeof(buf)); 
    bzero(will_send,999999);
    first_node=find_first_node_in_path(buf);
    end_node=find_end_node_in_path(buf);
    
    /* READER SECTION */
    /* equal priority */
    if(rw_priority==2){
        pthread_mutex_lock(&rw_mutex);
    }
    /* writer priority */
    if(rw_priority==1){
        pthread_mutex_lock(&rw_mutex);
        while ((active_writers+waiting_writers)>0){
            ++waiting_readers;
            pthread_cond_wait(&ok_to_read,&rw_mutex);
            --waiting_readers;
        }
    }
    /* reader priority */
    else if(rw_priority==0){
        pthread_mutex_lock(&rw_mutex);
        while((active_writers+active_readers)>0){
            ++waiting_readers;
            pthread_cond_wait(&ok_to_read,&rw_mutex);
            --waiting_readers;
        }
    }
    
    ++active_readers;
    pthread_mutex_unlock(&rw_mutex);
    sprintf(will_print,"[%lu]Thread #%d: searching database for a path from node %d to node %d\n",get_time_microseconds(),thread_index,first_node,end_node);
    print_string_output_fd(will_print,output_fd);
    /* Access the data structure in reader section in thread */
    cache_path=find_path_in_cache(first_node,end_node,cache);
    pthread_mutex_lock(&rw_mutex);
    --active_readers;

    /* equal priority */
    if(rw_priority==2){
        pthread_mutex_unlock(&rw_mutex);
    }
    /* writer priority */
    if(rw_priority==1){
        if(active_readers==0 && waiting_writers >0){
            pthread_cond_signal(&ok_to_write);
        }
        pthread_mutex_unlock(&rw_mutex);
    }
    /* reader priority */
    else if(rw_priority==0){
        if(waiting_readers>0){
            pthread_cond_signal(&ok_to_read);
        }
        else if(waiting_writers>0){
            pthread_cond_broadcast(&ok_to_write);
        }
        pthread_mutex_unlock(&rw_mutex);
    }
    
    /* READER SECTION END */

    /* If it finds path in cache */
    if(cache_path!=NULL){
        sprintf(will_print,"[%lu]Thread #%d: path found in database: %s\n",get_time_microseconds(),thread_index,cache_path);
        print_string_output_fd(will_print,output_fd);
        /* Sends the path to client */
        write(sockfd, cache_path, strlen(cache_path));
    }
    /* If it does not find path in cache */
    /* Calculates the new path */
    else{
        sprintf(will_print,"[%lu]Thread #%d: no path in database, calculating %d->%d\n",get_time_microseconds(),thread_index,first_node,end_node);
        print_string_output_fd(will_print,output_fd);
        /* If it finds path in graph */
        if(find_path_in_graph(graph,first_node,end_node,will_send)){
            
            sprintf(will_print,"[%lu]Thread #%d: path calculated: %s\n",get_time_microseconds(),thread_index,will_send);
            print_string_output_fd(will_print,output_fd);

            /* WRITER SECTION */
            /* equal priority */
            if(rw_priority==2){
                pthread_mutex_lock(&rw_mutex);
            }
            /* writer priority */
            if(rw_priority==1){
                pthread_mutex_lock(&rw_mutex);
                while((active_writers+active_readers)>0){
                    ++waiting_writers;
                    pthread_cond_wait(&ok_to_write,&rw_mutex);
                    --waiting_writers;
                }
            }
            /* reader priority */
            else if(rw_priority==0){
                pthread_mutex_lock(&rw_mutex);
                while((active_readers+waiting_readers)>0){
                    ++waiting_writers;
                    pthread_cond_wait(&ok_to_write,&rw_mutex);
                    --waiting_writers;
                }
            }
            ++active_writers;
            pthread_mutex_unlock(&rw_mutex);
            sprintf(will_print,"[%lu]Thread #%d: responding to client and adding path to database\n",get_time_microseconds(),thread_index);
            print_string_output_fd(will_print,output_fd);
            /* Writes the cache */
            add_path_in_cache(will_send,cache,output_fd);
            pthread_mutex_lock(&rw_mutex);
            --active_writers;

            /* equal priority */
            if(rw_priority==2){
                pthread_mutex_unlock(&rw_mutex);
            }
            /* writer priority */
            if(rw_priority==1){
                if(waiting_writers>0){
                pthread_cond_signal(&ok_to_write);
                }
                else if(waiting_readers>0){
                    pthread_cond_broadcast(&ok_to_read);
                }
                pthread_mutex_unlock(&rw_mutex);
            }
            /* reader priority */
            else if(rw_priority==0){
                if(active_writers==0 && waiting_readers >0){
                    pthread_cond_signal(&ok_to_read);
                }
                pthread_mutex_unlock(&rw_mutex);
            }
            /* WRITER SECTION END*/

            /* Sends the path to client */
            write(sockfd, will_send, strlen(will_send)); 
        }
        /* If it does not find path in graph */
        else{
            sprintf(will_send,"NO PATH");
            sprintf(will_print,"[%lu]Thread #%d: path not possible from node %d to %d\n",get_time_microseconds(),thread_index,first_node,end_node);
            print_string_output_fd(will_print,output_fd);
            /* Sends the path to client */
            write(sockfd, will_send, strlen(will_send));
        }
    }
    free(will_send);
    
} 

/* Opens a new socket */
void open_socket(int port){
    struct sockaddr_in server_addres; 

    /* Socket create */
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        print_string_output_fd("server | main | socket creation failed.\n",output_fd); 
        unlink("running");
        exit(EXIT_FAILURE); 
    } 
    bzero(&server_addres, sizeof(server_addres)); 
  
    /* assigning ip and port */ 
    server_addres.sin_family = AF_INET; 
    server_addres.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addres.sin_port = htons(port); 
  
    /* Binds the created socket to given ip and port */
    if ((bind(sockfd, (struct sockaddr*)&server_addres, sizeof(server_addres))) != 0) { 
        print_string_output_fd("server | main | socket bind failed.\n",output_fd); 
        unlink("running");
        exit(EXIT_FAILURE);
    } 
  
    /* Server listens */
    if ((listen(sockfd, 4096)) != 0) { 
        print_string_output_fd("server | main | server listen failed.\n",output_fd); 
        unlink("running");
        exit(EXIT_FAILURE); 
    } 
    len = sizeof(cli);
}

/* Thread function that processes connections (accepted fds) from the main thread and returns to the client */
void *calculator_threads (void *arg){
    int thread_index=*((int *)arg),client_fd;
    char will_print[200];
    sprintf(will_print,"[%lu]Thread #%d: Waiting for the connection.\n",get_time_microseconds(),thread_index);
    print_string_output_fd(will_print,output_fd);
    while(1){
        /* Locks the thread's mutex */
        pthread_mutex_lock(&mutexes[thread_index]);

        /* If the queue is empty and sigint_flag is zero, then waits */
        while(is_empty(socketfd_queues[thread_index]) && sigint_flag==0){
            pthread_cond_wait(&conditions_empty[thread_index],&mutexes[thread_index]);
        }

        /* Finishing condition (SIGINT) */
        if(sigint_flag){
            int i;
            pthread_mutex_unlock(&mutexes[thread_index]);
            sprintf(will_print,"[%lu]Thread %d is exiting.\n",get_time_microseconds(),thread_index);
            print_string_output_fd(will_print,output_fd);
            /* Sends signals for exiting other threads */
            for(i=0;i<total_thread_size;++i){
                if(pthread_cond_signal(&conditions_empty[i])!=0){
                    print_string_output_fd("server | calculator_threads | pthread_cond_signal error.\n",output_fd);
                    unlink("running");
                    exit(EXIT_FAILURE);
                }
            }
            break;
        }
        /* Pops the fd in the thread's queue */
        client_fd=get_item_and_delete(socketfd_queues[thread_index]);

        ++running_thread_size;

        /* Get and send the path processes */
        get_request(client_fd,thread_index);

        /* Closes the accepted fd */
        close(client_fd);

        /* Unlocks the thread's mutex */
        pthread_mutex_unlock(&mutexes[thread_index]);

        --running_thread_size;
    }

    return NULL;
}

/* Thread function that increases the number of threads by 25% if it looks at the thread size and operates at over 75% */
void *pool_thread(void *arg){
    int limit_thread=*((int*)arg);
    int i,total_increased_size=0;
    double increase_amount=0,system_load;
    char will_print[500];

    while(1){
        /* Locks the thread's pool mutex */
        pthread_mutex_lock(&pool_mutex);

        /* Function waits for thread size if it is below 75% or at full upper limit */
        while(((system_load=(100*((double)running_thread_size/(double)total_thread_size)))<75 || total_thread_size>=limit_thread || running_thread_size==total_thread_size) && sigint_flag==0){
            pthread_cond_wait(&pool_cond,&pool_mutex);
        }

        /* Finishing condition */
        if(sigint_flag){
            pthread_mutex_unlock(&pool_mutex);
            sprintf(will_print,"[%lu]Pool thread is exiting.\n",get_time_microseconds());
            print_string_output_fd(will_print,output_fd);
            break;
        }

        /* Calculation of increased size */
        increase_amount=((double)total_thread_size)/4.0;
        total_thread_size+=(atomic_int)increase_amount;
        if(total_thread_size>limit_thread){
            increase_amount-=total_thread_size-limit_thread;
            total_thread_size=limit_thread;
        }
        total_increased_size+=(atomic_int)increase_amount;
        
        sprintf(will_print,"[%lu]System load %f%c,pool extended to %d threads\n",get_time_microseconds(),system_load,37,total_thread_size);
        print_string_output_fd(will_print,output_fd);

        /* Extends the tid_threads array to creating new threads */
        tid_threads=(pthread_t*)realloc(tid_threads,total_thread_size*sizeof(pthread_t));
        //thread_no=(int*)realloc(thread_no,total_thread_size*sizeof(int));

        /* New thread's creation */
        for(i=total_thread_size-(int)increase_amount;i<total_thread_size;++i){
            thread_no [i] = i;
            if (pthread_create (&tid_threads [i], NULL, calculator_threads, (void *) &thread_no [i]) != 0) { 
                print_string_output_fd ("server | main | pthread_create error\n",output_fd); 
                unlink("running");
                exit (EXIT_FAILURE);
            }
        }

        /* Pool mutex unlocks */
        pthread_mutex_unlock(&pool_mutex);
    }
    return NULL;
}

/* SIGINT handler function */
void sigint_handler(int signum){
    char will_print[500];
    sprintf(will_print,"[%lu]Termination signal(%d) received, waiting for ongoing threads to complete.\n",get_time_microseconds(),signum);
    print_string_output_fd(will_print,output_fd);
    sigint_flag=1;
}

/* The files are opened, the daemon process is created, then the sockets are opened and after all threads are started, main thread runs until the sigint signal comes */
int main(int argc,char *argv[]){
    int opt,running_fd;
    char input_file_name[500],output_file_name[500],will_print[1000];
    int port,nof_threads,max_threads,i;
    unsigned long time;
    int accepted_fd;

    /* Handler settings */
    struct sigaction sact;
    sigemptyset(&sact.sa_mask); 
    sact.sa_flags = 0;
    sact.sa_handler = sigint_handler;
    sigint_flag=0;

    /* Avoiding second running of server program : The running file is created while the program is running. If there is a running file, if the new program is tried to run, it gives an error.*/
    if(access("running",F_OK)!=-1){
        print_error("server | main | You can not run second server program!\n");
        exit(EXIT_FAILURE);
    }
    else{
        if((running_fd=open("running",O_CREAT,0777))<0){
            print_error("server | main | Open error.\n");
            exit(EXIT_FAILURE);
        }
    }


    while((opt = getopt(argc, argv, ":i:p:o:s:x:r:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i':
                strcpy(input_file_name,optarg);
                break;
            case 'p':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57){
                        print_error("server | main | -p flag must be non negative number\n");
                        unlink("running");
                        exit(EXIT_FAILURE);
                    }
                }
                port=atoi(optarg);
                break;
            case 'o':
                strcpy(output_file_name,optarg);
                if(strcmp(output_file_name,input_file_name)==0){
                    print_error("server | main | the files must not be same!\n");
                    unlink("running");
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57){
                        print_error("server | main | -s flag must be non negative number\n");
                        unlink("running");
                        exit(EXIT_FAILURE);
                    }
                }
                nof_threads=atoi(optarg);
                if(nof_threads<2){
                    print_error("server | main | Invalid s size!\n");
                    unlink("running");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'x':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57){
                        print_error("server | main | -x flag must be non negative number\n");
                        unlink("running");
                        exit(EXIT_FAILURE);
                    }
                }
                max_threads=atoi(optarg);
                if(max_threads<nof_threads){
                    print_error("server | main | -x must not be lower than s!\n");
                    unlink("running");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'r':
                rw_priority=atoi(optarg);
                if(rw_priority!=0 && rw_priority!=1 && rw_priority!=2){
                    print_error("server | main | -r must be 0,1 or 2!\n");
                    unlink("running");
                    exit(EXIT_FAILURE);
                }
                break;
            case ':':  
                errno=EINVAL;
                print_error("server | main | You must write like this: \"./server -i pathToFile -p PORT -o pathToLogFile -s 4 -x 24 -r PRIO\"\n");
                unlink("running");
                exit(EXIT_FAILURE);
                break;  
            case '?':  
                errno=EINVAL;
                print_error("server | main | You must write like this: \"./server -i pathToFile -p PORT -o pathToLogFile -s 4 -x 24 -r PRIO\"\n");
                unlink("running");
                exit(EXIT_FAILURE);
                break; 
            default:
                abort (); 
        }
    }

    /* Controls are there more or less parameters */
    if(optind!=13){
        errno=EINVAL;
        print_error("server | main | You must write like this: \"./server -i pathToFile -p PORT -o pathToLogFile -s 4 -x 24 -r PRIO\"\n");
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Opens the log file */
    output_fd=open(output_file_name,O_WRONLY | O_CREAT | O_TRUNC,0777);
    if( output_fd<0){
        print_error("server | main | open file error.\n");
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Creates the daemon */
    create_daemon();

    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &sact, NULL) != 0){
        print_string_output_fd("server | main | SIGINT sigaction() error\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }

    sprintf(will_print,"[%lu]Executing with parameters:\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);
    sprintf(will_print,"[%lu]-i %s\n",get_time_microseconds(),input_file_name);
    print_string_output_fd(will_print,output_fd);
    sprintf(will_print,"[%lu]-p %d\n",get_time_microseconds(),port);
    print_string_output_fd(will_print,output_fd);
    sprintf(will_print,"[%lu]-o %s\n",get_time_microseconds(),output_file_name);
    print_string_output_fd(will_print,output_fd);
    sprintf(will_print,"[%lu]-s %d\n",get_time_microseconds(),nof_threads);
    print_string_output_fd(will_print,output_fd);
    sprintf(will_print,"[%lu]-x %d\n",get_time_microseconds(),max_threads);
    print_string_output_fd(will_print,output_fd);
    sprintf(will_print,"[%lu]-r %d\n",get_time_microseconds(),rw_priority);
    print_string_output_fd(will_print,output_fd);

    /* Initializing the global variables */
    tid_threads=(pthread_t*)calloc(nof_threads,sizeof(pthread_t));
    thread_no=(int*)calloc(max_threads,sizeof(int));
    mutexes=(pthread_mutex_t*)calloc(max_threads,sizeof(pthread_mutex_t));
    conditions_empty=(pthread_cond_t*)calloc(max_threads,sizeof(pthread_cond_t));
    running_thread_size=active_readers=active_writers=waiting_readers=waiting_writers=0;
    total_thread_size=nof_threads;

    sprintf(will_print,"[%lu]Loading graph...\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);
    time=get_time_microseconds();
    /* Loads the graph */
    graph=initialize_graph(input_file_name,output_fd);

    sprintf(will_print,"[%lu]Graph loaded in %f seconds with %d nodes and %d edges.\n",get_time_microseconds(),((double)get_time_microseconds()-time)/1000000,graph->matrix_node,graph->matrix_edge);
    print_string_output_fd(will_print,output_fd);

    /* Creates the cache database */
    cache=create_cache(graph->matrix_node);

    /* creates the inter-thread queues */
    socketfd_queues=(struct Queue**)calloc(max_threads,sizeof(struct Queue*));
    for(i=0;i<max_threads;++i){
        if(graph->matrix_node<10000){
            socketfd_queues[i]=create_queue(10000);
        }
        else{
            socketfd_queues[i]=create_queue(graph->matrix_node);
        }
    }

    /* opens the socket */
    open_socket(port);

    /* Mutex initializations */
    for(i=0;i<max_threads;++i){
        if(pthread_mutex_init(&mutexes[i],NULL)!=0){
            print_string_output_fd("server | main | Mutex initialize error\n",output_fd);
            unlink("running");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_mutex_init(&rw_mutex,NULL)!=0 || pthread_mutex_init(&pool_mutex,NULL)!=0 ){
        print_string_output_fd("server | main | Mutex initialize error\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Condition variable initialization */
    for(i=0;i<max_threads;++i){
        if(pthread_cond_init(&conditions_empty[i],NULL)!=0){
            print_string_output_fd("server | main | Condition initialize error\n",output_fd);
            unlink("running");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_cond_init(&ok_to_read,NULL)!=0 || pthread_cond_init(&ok_to_write,NULL)!=0 ||  pthread_cond_init(&pool_cond,NULL)!=0){
        print_string_output_fd("server | main | Condition initialize error\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Creates threads */
    sprintf(will_print,"[%lu]A pool of %d threads has been created\n",get_time_microseconds(),nof_threads);
    print_string_output_fd(will_print,output_fd);
    for (i = 0; i < nof_threads; ++i) {
        thread_no [i] = i;
        if (pthread_create (&tid_threads [i], NULL, calculator_threads, (void *) &thread_no [i]) != 0) { 
            print_string_output_fd ("server | main | pthread_create error\n",output_fd); 
            unlink("running");
            exit (EXIT_FAILURE);
        }
    }
    if(pthread_create(&pool_thread_id,NULL,pool_thread,(void*)&max_threads)!=0){
        print_string_output_fd ("server | main | pthread_create error\n",output_fd); 
        unlink("running");
        exit (EXIT_FAILURE);
    }

    /* Main thread */
    i=0;
    /* Continues until the sigint flag equal to 1 */
    while(sigint_flag==0){ 
        /* Accepts the connection */
        accepted_fd = accept(sockfd, (struct sockaddr*)&cli, &len); 

        /* locks the ith thread's mutex and pool mutex */
        pthread_mutex_lock(&mutexes[i]);
        pthread_mutex_lock(&pool_mutex);
        
        if(sigint_flag==0){
            if (accepted_fd < 0) { 
                print_string_output_fd("server | main | server accept failed.\n",output_fd); 
                unlink("running");
                exit(EXIT_FAILURE);
            } 
            else{
                sprintf(will_print,"[%lu]A connection has been delegated to thread id #%d, system load %f%c\n",get_time_microseconds(),i,100*((double)running_thread_size/(double)total_thread_size),37);
                print_string_output_fd(will_print,output_fd);
                pthread_cond_signal(&pool_cond);
            }
        }  
        if(running_thread_size==total_thread_size){
            sprintf(will_print,"[%lu]No thread is available! Waiting for one\n",get_time_microseconds());
            print_string_output_fd(will_print,output_fd);
        }
        /* add item the queue and sends signal signal */
        add_item(socketfd_queues[i],accepted_fd);
        pthread_cond_signal(&conditions_empty[i]);

        /* unlocks the ith thread's mutex and pool mutex */
        pthread_mutex_unlock(&pool_mutex);
        pthread_mutex_unlock(&mutexes[i]);
;  
        ++i;
        i=i%total_thread_size;
    } 

    /* Sends signal to close threads in sigint */
    for(i=0;i<total_thread_size;++i){
        if(pthread_cond_signal(&conditions_empty[i])!=0){
            print_string_output_fd("server | main | pthread_cond_signal error.\n",output_fd);
            unlink("running");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_cond_signal(&pool_cond)!=0){
        print_string_output_fd("server | main | pthread_cond_signal error.\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Wait(join) for threads to terminate */
    for (i = 0; i < total_thread_size; ++i){
        if (pthread_join (tid_threads [i], NULL) == -1) { 
            print_string_output_fd ("server | main | pthread_join error\n",output_fd); 
            unlink("running");
            exit (EXIT_FAILURE);
        }
    }
    if (pthread_join (pool_thread_id, NULL) == -1) { 
        print_string_output_fd ("server | main | pthread_join error\n",output_fd); 
        unlink("running");
        exit (EXIT_FAILURE);
    }
    sprintf(will_print,"[%lu]All threads have terminated, server shutting down\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);

    /* Closes the socket */
    if(close(sockfd)<0){
        print_string_output_fd("server | main | close error\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    } 
    sprintf(will_print,"[%lu]Sockets closed.\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);

    /* Destroys the mutexes */
    for(i=0;i<max_threads;++i){
        if(pthread_mutex_destroy(&mutexes[i])!=0){
			pthread_mutex_unlock(&mutexes[i]);
			if(pthread_mutex_destroy(&mutexes[i])!=0){
				print_string_output_fd("server | main | Mutex destroy error\n",output_fd);
            	unlink("running");
                exit(EXIT_FAILURE);
			}
        }
    }
    if(pthread_mutex_destroy(&rw_mutex)!=0 || pthread_mutex_destroy(&pool_mutex)!=0){
        print_string_output_fd("server | main | Mutex destroy error\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }
    sprintf(will_print,"[%lu]Mutexes destroyed.\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);
    
    /* Destroys the condition variable */
    for(i=0;i<max_threads;++i){
        if(pthread_cond_destroy(&conditions_empty[i])!=0){
            print_string_output_fd("server | main | Condition destroy error\n",output_fd);
            unlink("running");
            exit(EXIT_FAILURE);
        }
    }
    if(pthread_cond_destroy(&ok_to_read)!=0 || pthread_cond_destroy(&ok_to_write)!=0 || pthread_cond_destroy(&pool_cond)!=0){
        print_string_output_fd("server | main | Condition destroy error\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }
    sprintf(will_print,"[%lu]Condition variables destroyed.\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);

    /* Frees the all heap variables */
    free_graph(graph);
    free_cache(cache);
    free(tid_threads);
    free(thread_no);
    free(mutexes);
    free(conditions_empty);
    for(i=0;i<max_threads;++i){
        free_queue(socketfd_queues[i]);
    }
    free(socketfd_queues);
    sprintf(will_print,"[%lu]Variables free'd.\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);

    sprintf(will_print,"[%lu]Files are closing and deleting...\n",get_time_microseconds());
    print_string_output_fd(will_print,output_fd);

    /* Closes the used files */
    if(close(output_fd)<0){
        print_string_output_fd("server | main | Close file error\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }

    /* Unlinks the "running" file */
    if(unlink("running")==-1){
        print_string_output_fd("server | main | Unlink file error\n",output_fd);
        exit(EXIT_FAILURE);
    }
    
    /* Exits success */
    exit(EXIT_SUCCESS);

}
