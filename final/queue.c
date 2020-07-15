#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include "input_output.h"

/* Creates the queue and returns it */
struct Queue* create_queue(int capacity) 
{ 
    struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue)); 
    queue->capacity=capacity; 
    queue->front=0; 
    queue->size=0;
    queue->rear=-1;
    queue->array = (int*)calloc(queue->capacity,sizeof(int)); 
    return queue;
} 

/* Controls is empty */
int is_empty(struct Queue* queue){
    return queue->size==0;
}

/* Controls is full */
int is_full(struct Queue* queue){
    int ret_val=(queue->front+queue->size)==queue->capacity;
    if(ret_val==1){
        int i;
        for(i=0;i<queue->size;++i){
            queue->array[i]=queue->array[queue->front+i];
        }
        queue->rear=queue->size-1;
        queue->front=0;
    }
    return ret_val;
}

/* Adds item to rear */
int add_item(struct Queue* queue,int item){
    if(is_full(queue)){
        queue->array=(int*)realloc(queue->array,(queue->capacity+1000)*sizeof(int));
        queue->capacity+=1000;
    }
    ++queue->rear;
    queue->array[queue->rear]=item;
    ++queue->size;
    return queue->size;
}

/* Gets item in the front */
int get_item(struct Queue* queue){
    return queue->array[queue->front];
}

/* Gets item in the front and deletes it (pop) */
int get_item_and_delete(struct Queue* queue){
    int item=queue->array[queue->front++];
    --queue->size;
    return item;
}

/* Frees the queue structure */
void free_queue(struct Queue* queue){
    free(queue->array);
    free(queue);
}
