#include "queue.h"

#ifndef SUDOKU_COMMON_H
#define SUDOKU_COMMON_H

typedef struct {
    int content[9][9];
} puzzle;

typedef struct {
    int row;
    int column;
} location;

typedef struct {
    int puzzle_id;
    FILE *input_file;
    FILE *output_file;
} sudoku_threads_input;

typedef struct {
    FILE *input_file;
    FILE *output_file;
    Queue *input_queue;
    Queue *output_queue;
    int *read_complete_flag_ptr;
    int *solve_complete_flag_ptr;
} sudoku_workers_input;

typedef struct {
    Queue *puzzle_queue;
    FILE *output_file;
    int *result_found_flag_ptr;
    int *no_more_puzzles_flag_ptr;
} sudoku_multi_input;

int init_locks();

puzzle *read_next_puzzle(FILE *inputfile);
puzzle *read_next_puzzle_with_lock(FILE *inputfile);

void write_to_file(puzzle *p, FILE *outputfile);
void write_to_file_with_lock(puzzle *p, FILE *outputfile);

void *print_puzzle(puzzle *p);

#endif //SUDOKU_COMMON_H
