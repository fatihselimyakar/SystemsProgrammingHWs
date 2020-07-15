#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include "input_output.h"
#include "cache.h"

/* Creates the cache stucture and returns */
struct Cache* create_cache(int capacity){
    struct Cache* cache=(struct Cache*)calloc(1,sizeof(struct Cache));
    cache->array=(struct CacheNode*)calloc(capacity,sizeof(struct CacheNode));
    cache->capacity=capacity;
    cache->size=0;
    return cache;
}

/* Finds the end node of char* path by seperating tokens */
int find_end_node_in_path(char *path){
    char *string=(char*)calloc(strlen(path)+1,sizeof(char));
    int ret_val;
    strncpy(string,path,strlen(path)+1);

    char *token=strtok(string,"-> \n\t");
    while(token!=NULL){
        ret_val=atoi(token);
        token=strtok(NULL,"-> \n\t");
    }
    free(string);
    return ret_val;
}

/* Finds the first node of char* path by seperating tokens */
int find_first_node_in_path(char *path){
    char *string=(char*)calloc(strlen(path)+1,sizeof(char));
    int ret_val;
    strncpy(string,path,strlen(path)+1);

    char *token=strtok(string,"-> \n\t");
    ret_val=atoi(token);

    free(string);
    return ret_val;
}

/* Add new path in the linkedlist array */
void add_path_in_cache(char* path,struct Cache* cache,int output_fd){
    int index=find_first_node_in_path(path);
    if(index>cache->capacity-1){
        print_string_output_fd("Index is invalid in add_path_in_cache\n",output_fd);
        unlink("running");
        exit(EXIT_FAILURE);
    }
    struct CacheNode* item=&cache->array[index];
    while(item->next!=NULL){
        item=item->next;
    }
    item->next=(struct CacheNode*)calloc(1,sizeof(struct CacheNode));
    item=item->next;

    item->path=(char*)calloc(strlen(path)+1,sizeof(char));
    strncpy(item->path,path,strlen(path)+1);
    item->end_node=find_end_node_in_path(path);
    item->next=NULL;

    ++cache->size;
}

/* Finds the path in the cache */
char* find_path_in_cache(int start_node,int end_node,struct Cache* cache){
    /* if path is not exist */
    if(start_node>cache->capacity-1 || end_node>cache->capacity-1){
        return NULL;
    }
    struct CacheNode* item=(&cache->array[start_node])->next;
    while(item!=NULL){
        /* if path is exist */
        if(item->end_node==end_node){
            return item->path;
        }
        item=item->next;
    }
    /* if path is not exist */
    return NULL;

}


/* Frees the cache data structure */
void free_cache(struct Cache* cache){
     int i;
     for(i=0;i<cache->capacity;++i){
        struct CacheNode* item=&(cache->array[i]);
        struct CacheNode* old_item;
        int i=0;
        while(item!=NULL){
            old_item=item;
            item=item->next;
            free(old_item->path);
            if(i!=0)
                free(old_item);
            ++i;
        }
     }
     free(cache->array);
     free(cache);
}

