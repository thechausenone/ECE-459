/*****************************************************************************
 **
 ** queue.h
 ** 
 ** Function implementations for Queue, a polymorphic FIFO data structure
 ** 
 ** Author: Sean Butze, 2016
 ** Modifications: David Chau, 2020
 **
 ****************************************************************************/

#include <pthread.h>

#ifndef QUEUE_H
#define QUEUE_H

typedef struct Node {
    void *val;
    struct Node *next;
} Node;

typedef struct Queue {
    unsigned size;
    Node *head;
    Node *tail;
    pthread_mutex_t lock;
} Queue;

extern Queue* Queue_init();
extern void Queue_add(Queue *q, void *el);
extern void* Queue_remove(Queue *q);
extern int Queue_size(Queue *q);
extern void Queue_delete(Queue *q);

#endif