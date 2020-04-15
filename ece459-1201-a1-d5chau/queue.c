/*****************************************************************************
 **
 ** queue.c
 ** 
 ** Function implementations for Queue, a polymorphic FIFO data structure
 ** 
 ** Author: Sean Butze, 2016
 ** Modifications: David Chau, 2020
 **
 ****************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include "queue.h"

Queue* Queue_init() {
    Queue *q = (Queue*) malloc(sizeof(Queue));
    q->size = 0;
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&(q->lock), NULL);
    return q;
}

void Queue_add(Queue *q, void *el) {
    pthread_mutex_lock(&(q->lock));
    Node *temp = q->head;
    Node *new_tail = malloc(sizeof(Node));
    new_tail->val = el;
    new_tail->next = NULL;

    if (q->size < 1) {
        q->head = new_tail;
    } else {
        q->tail->next = new_tail;
    }

    q->tail = new_tail;
    q->size++;
    pthread_mutex_unlock(&(q->lock));
}

void* Queue_remove(Queue *q) {
    pthread_mutex_lock(&(q->lock));
    assert(q && q->size > 0);
    Node *head = q->head;
    void *headval = head->val;
    q->head = head->next;

    if (q->size == 1) {
        q->tail = NULL; // Prevent dangling pointer
    }

    q->size--;
    free(head);
    pthread_mutex_unlock(&(q->lock));
    return (headval);
}

int Queue_size(Queue *q) {
    pthread_mutex_lock(&(q->lock));
    int result = q->size;
    pthread_mutex_unlock(&(q->lock));
    return result;
}

void Queue_delete(Queue *q) {
    assert(q);
    Node *i1, *i2;

    for (i1 = q->head; i1; i1 = i2) {
        i2 = i1->next;
        free(i1->val);
        free(i1);
    }
    
    pthread_mutex_destroy(&(q->lock));
    free(q);
}
