
#ifndef QUEUE_H_   /* Include guard */
#define QUEUE_H_

/* Queue data structre */
struct Queue
{
    int front,rear,size;
    int capacity;
    int *array;
};

/* Queue functions */
struct Queue* create_queue(int capacity);
int is_empty(struct Queue* queue);
int is_full(struct Queue* queue);
int add_item(struct Queue* queue,int item);
int get_item(struct Queue* queue);
int get_item_and_delete(struct Queue* queue);
void free_queue(struct Queue* queue);

#endif // QUEUE_H_
