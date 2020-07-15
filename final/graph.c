#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "graph.h"
#include "input_output.h"
#include "queue.h"

/* Finds and returns the maximum indexed node to determine the number of nodes */
int find_maximum_index(char* file_name,int output_fd){
    int i=0,max=0;
    FILE *stream=fopen(file_name,"r");
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    
    /* Seperates tokens and finds max node value */
    do{   
        nread = getline(&line, &len, stream);
        if(i==0 && nread==-1){
            print_string_output_fd("The input file must not be empty!\n",output_fd);
            unlink("running");
            exit(EXIT_FAILURE);
        }
        else if(line[0]!='#' && nread!=0){
            char* token = strtok(line, " #();:,\n\t");
            int token_ct=0,x,y; 
            while (token != NULL) {
                if(token_ct==0){
                    x=atoi(token);
                    if(x>max)
                        max=x;
                }
                else if(token_ct==1){
                    y=atoi(token);
                    if(y>max)
                        max=y;
                }
                else if(token_ct>=2){
                    print_string_output_fd("There is an error in input file\n",output_fd);
                    unlink("running");
                    exit(EXIT_FAILURE);
                }
                token = strtok(NULL, " #();:,\n\t"); 
                ++token_ct;
            }
        }
        ++i;
    }
    while (nread!= -1 );
    free(line);
    fclose(stream);

    return max;
}

/* Initializes the graph reading line by line */
struct Graph* initialize_graph(char* file_name,int output_fd){
    FILE *stream=fopen(file_name,"r");
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    int i=0;
    struct Graph* graph=(struct Graph*)malloc(sizeof(struct Graph));
    graph->matrix_node=find_maximum_index(file_name,output_fd)+1;
    graph->size=0;
    graph->size_matrix=(int*)calloc(graph->matrix_node,sizeof(int));
    graph->matrix=(int**)calloc(graph->matrix_node,sizeof(int*));

    do{
        /* Reads 1 line */
        nread = getline(&line, &len, stream);
        if(i==0 && nread==-1){
            print_string_output_fd("The input file must not be empty!\n",output_fd);
            unlink("running");
            exit(EXIT_FAILURE);
        }
        /* Ignores # character */
        else if(line[0]!='#' && nread!=0){
            /* Seperates tokens */
            char* token = strtok(line, " #();:,\n\t");
            int token_ct=0,x,y; 
            while (token != NULL) {
                if(token_ct==0){
                    x=atoi(token);
                    if(x<0){
                        print_string_output_fd("Node must not be negative number\n",output_fd);
                        unlink("running");
                        exit(EXIT_FAILURE);
                    }
                }
                else if(token_ct==1){
                    y=atoi(token);
                    if(y<0){
                        print_string_output_fd("Node must not be negative number\n",output_fd);
                        unlink("running");
                        exit(EXIT_FAILURE);
                    }
                        
                }
                else if(token_ct>=2){
                    print_string_output_fd("There is an error in input file\n",output_fd);
                    unlink("running");
                    exit(EXIT_FAILURE);
                }
                token = strtok(NULL, " #();:,\n\t"); 
                ++token_ct;
            }
            /* Adds found nodes */
            add_node(x,y,graph);
            ++i;
        }
    }
    while (nread!= -1 );
    free(line);
    
    graph->matrix_edge=i-1;

    fclose(stream);

    return graph;
}

/* Adds node in graph */
int add_node(int first_node,int second_node,struct Graph* graph){
    if(graph->size_matrix[first_node]==0){
        graph->matrix[first_node]=(int*)calloc(1,sizeof(int));
    }
    else{
        graph->matrix[first_node]=(int*)realloc(graph->matrix[first_node],(graph->size_matrix[first_node]+1)*sizeof(int));
    }
    graph->matrix[first_node][graph->size_matrix[first_node]]=second_node;
    ++graph->size_matrix[first_node];
    return ++graph->size; 
}

/* Control is there node in graph */
int is_there_node(int first_node,int second_node,struct Graph* graph){
    int i;
    for(i=0;i<graph->size_matrix[first_node];++i){
        if(graph->matrix[first_node][i]==second_node){
            return 1;
        }
    }
    return 0;
}


/* BFS that stores the parents */
int BFS(struct Graph* graph, int s, int d, int* parent_array){ 
    struct Queue *queue=create_queue(graph->matrix_node);
    int* visited=(int*)calloc(graph->matrix_node,sizeof(int));

	for (int i = 0; i < graph->matrix_node; i++) { 
		visited[i] = 0; 
		parent_array[i] = -1; 
	} 

    /* Marks first node visited and pushes the queue */
	visited[s] = 1; 
    add_item(queue,s);

	/* BFS algorithm */
	while (!is_empty(queue)) { 
		int u = get_item_and_delete(queue);
		for (int i = 0; i < graph->size_matrix[u]; ++i) { 
            /* If the source node and destination node is same */
            if(s==d && graph->matrix[u][i]==s){
                parent_array[graph->matrix[u][i]] = u;
                free_queue(queue);
                free(visited);
                return 1; 
            }  
            /* If the node is not visited adds the queue and marks visited*/
            if(visited[graph->matrix[u][i]] == 0){
                visited[graph->matrix[u][i]]=1;
				parent_array[graph->matrix[u][i]] = u; 
                add_item(queue,graph->matrix[u][i]);

                /* If found then exits */
				if (graph->matrix[u][i] == d){
                    free_queue(queue);
                    free(visited);
                    return 1; 
                }
					
            }
            
		} 
	} 
    /* Not found */
    free_queue(queue);
    free(visited);
	return 0; 
} 

/* Cover function of BFS */
int find_path_in_graph(struct Graph* graph, int s, int d,char *return_path){ 
    if(s>=graph->matrix_node || d>=graph->matrix_node){
        return 0;
    }
    int* parent_array=(int*)calloc(graph->matrix_node,sizeof(int));
    int limit=999999,limited_index_in_pred=-1;


	if (BFS(graph,s,d,parent_array) == 0) { 
        free(parent_array);
		return 0;
	} 

	/* Shortest path queue */
    /* Creates the path */
	struct Queue *path=create_queue(graph->matrix_node);
	int index = d; 
	add_item(path,index); 
    if(s==d){
        limited_index_in_pred=s;
    }
	while (parent_array[index] != limited_index_in_pred) { 
        add_item(path,parent_array[index]);
		index = parent_array[index]; 
	} 
    if(s==d){
        add_item(path,s);
    }

    /* Convert path to char* */
	for (int i = path->size - 1; i >= 0; i--){
        char string[50];
        if((int)strlen(return_path)>=limit-100){
            strcat(return_path,"...");
            break;
        }
        if(i!=0) 
		    sprintf(string,"%d->",path->array[i]);
        else
            sprintf(string,"%d",path->array[i]);
        strcat(return_path,string);
    }

    free_queue(path);
    free(parent_array);
    return 1;
} 

/* Frees the graph structure */
void free_graph(struct Graph* graph){
    int i;
    for(i=0;i<graph->matrix_node;++i){
        free(graph->matrix[i]);
    }
    free(graph->size_matrix);
    free(graph->matrix);
    free(graph);
}
