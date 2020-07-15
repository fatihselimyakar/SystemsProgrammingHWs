#ifndef GRAPH_H_  
#define GRAPH_H_

/* Graph data structure, 2d non-fixed size matrix*/
struct Graph 
{ 
    int** matrix;
    int* size_matrix;
    int matrix_node;
    int matrix_edge;
    int size;
};

/* Graph Functions */
int find_maximum_index(char* file_name,int output_fd);
int add_node(int first_node,int second_node,struct Graph* graph);
int is_there_node(int first_node,int second_node,struct Graph* graph);
struct Graph* initialize_graph(char* file_name,int output_fd);
int BFS(struct Graph* graph, int src, int dest, int* pred);
int find_path_in_graph(struct Graph* graph, int src, int dest,char* return_path);
void free_graph(struct Graph* graph);
#endif // GRAPH_H_
