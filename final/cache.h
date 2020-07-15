#ifndef CACHE_H_  
#define CACHE_H_

/* CacheNode structure */
struct CacheNode
{ 
    int end_node;
    char* path;
    struct CacheNode* next;
};

/* Cache structure */
struct Cache
{
    struct CacheNode* array;
    int capacity;
    int size;
};

/* Cache Functions */
struct Cache* create_cache(int capacity);
void add_path_in_cache(char* path,struct Cache* cache,int output_fd);
int find_end_node_in_path(char *path);
int find_first_node_in_path(char *path);
char* find_path_in_cache(int start_node,int end_node,struct Cache* cache);
void free_cache(struct Cache* cache);


#endif // CACHE_H_