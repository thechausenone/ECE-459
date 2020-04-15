#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "common.h"

pthread_mutex_t read_lock;
pthread_mutex_t write_lock;

/* Initialize read and write locks */
int init_locks() { 
    if (pthread_mutex_init(&read_lock, NULL) != 0)
    {
        printf("\n Mutex 'read_lock' init failed. \n");
        return 0;
    }
    
    if (pthread_mutex_init(&write_lock, NULL) != 0)
    {
        printf("\n Mutex 'write_lock' init failed. \n");
        return 0;
    }
    return 1;
}

/*
 * Function to read the next available puzzle from file
 */ 
puzzle *read_next_puzzle(FILE *inputfile) {
    puzzle *p = malloc(sizeof(puzzle));
    char temp[10];
    for (int i = 0; i < 9; i++) {
        int read = fscanf(inputfile, "%9c\n", temp);
        if (read != 1) {
            /* Reached EOF */
            free(p);
            return NULL;
        }
        for (int j = 0; j < 9; j++) {
            p->content[i][j] = temp[j] == '.'
                               ? 0
                               : temp[j] - '0';
        }
    }
    return p;
}

/*
 * Version of `read_next_puzzle` to be used in multi-threaded implementations
 */ 
puzzle *read_next_puzzle_with_lock(FILE *inputfile) {
    pthread_mutex_lock(&read_lock);
    puzzle *p = read_next_puzzle(inputfile);
    pthread_mutex_unlock(&read_lock);
    return p;
}

/*
 * Function to write a given puzzle to file
 */ 
void write_to_file(puzzle *p, FILE *outputfile) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if (8 == j) {
                fprintf(outputfile, "%d\n", p->content[i][j]);
            } else {
                fprintf(outputfile, "%d", p->content[i][j]);
            }
        }
    }
    fprintf(outputfile, "\n\n");
}

/*
 * Version of `write_to_file` to be used in multi-threaded implementations
 */ 
void write_to_file_with_lock(puzzle *p, FILE *outputfile) {
    pthread_mutex_lock(&write_lock);
    write_to_file(p, outputfile);
    pthread_mutex_unlock(&write_lock);
}

/*
 * Helper function that prints given puzzle to stdout
 */ 
void *print_puzzle(puzzle *p) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            printf("%d ", p->content[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}