/*
 * A simple backtracking sudoku solver.  Accepts input with cells, dot (.)
 * to represent blank spaces and rows separated by newlines. Output format is
 * the same, only solved, so there will be no dots in it.
 *
 * Copyright (c) Mitchell Johnson (ehntoo@gmail.com), 2012
 * Modifications 2019 by Jeff Zarnett (jzarnett@uwaterloo.ca) for the purposes
 * of the ECE 459 assignment.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include "common.h"
#include "queue.h"

/* Check the common header for the definition of puzzle */

/* Check if current number is valid in this position;
 * returns 1 if yes, 0 if not */
int is_valid(int number, puzzle *p, int row, int column);

int solve(puzzle *p, int row, int column);

void *read_handler(void *args);

void *solve_handler(void *args);

void *write_handler(void *args);

void set_solve_complete_flag(int *flag, int value);

int read_solve_complete_flag(int *flag);

void set_read_complete_flag(int *flag, int value);

int read_read_complete_flag(int *flag);

pthread_mutex_t solve_complete_flag_lock;

pthread_mutex_t read_complete_flag_lock;

int main(int argc, char **argv) {
    FILE *inputfile;
    FILE *outputfile;
    puzzle *p;
    int read_complete_flag = 0;
    int solve_complete_flag = 0;

    /* Parse arguments */
    int c;
    int num_threads = 1;
    char *filename = NULL;
    while ((c = getopt(argc, argv, "t:i:")) != -1) {
        switch (c) {
            case 't':
                num_threads = strtoul(optarg, NULL, 10);
                if (num_threads < 3) {
                    printf("%s: option requires an argument >= 3 -- 't'\n", argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'i':
                filename = optarg;
                break;
            default:
                return -1;
        }
    }

    /* Open Files */
    inputfile = fopen(filename, "r");
    if (inputfile == NULL) {
        printf("Unable to open input file.\n");
        return EXIT_FAILURE;
    }
    outputfile = fopen("output.txt", "w+");
    if (outputfile == NULL) {
        printf("Unable to open output file.\n");
        return EXIT_FAILURE;
    }

    /* Init mutex */
    pthread_mutex_init(&solve_complete_flag_lock, NULL);
    pthread_mutex_init(&read_complete_flag_lock, NULL);

    /* Setup arguments for handlers */
    sudoku_workers_input *args = (sudoku_workers_input *) malloc(sizeof(sudoku_workers_input));
    args->input_file = inputfile;
    args->output_file = outputfile;
    args->input_queue = Queue_init();
    args->output_queue = Queue_init();
    args->read_complete_flag_ptr = &read_complete_flag;
    args->solve_complete_flag_ptr = &solve_complete_flag;

    /* Create thread to `read_next_puzzle` */
    pthread_t reader_tid;
    pthread_create(&reader_tid, NULL, read_handler, (void*) args);

    /* Create thread to `write_to_file` */
    pthread_t writer_tid;
    pthread_create(&writer_tid, NULL, write_handler, (void*) args);

    /* Create threads to solve puzzles */
    int max_num_solvers = num_threads - 2;
    int num_solvers_active = 0;
    pthread_t solver_tids[max_num_solvers];

    /*
     * Solver threads will stop being created when these conditions are fulfilled:
     * - no more puzzles left in the input queue
     * - reader thread is done processing the input file
     */
    while (Queue_size(args->input_queue) != 0 || read_read_complete_flag(args->read_complete_flag_ptr) == 0) {
        int queue_size = Queue_size(args->input_queue);
        if (queue_size >= max_num_solvers) {
            num_solvers_active = max_num_solvers;
        }
        else {
            num_solvers_active = queue_size;
        }

        for (int i = 0; i < num_solvers_active; i++) {
            pthread_create(&solver_tids[i], NULL, solve_handler, (void*) args);
        }

        for (int i = 0; i < num_solvers_active; i++) {
            pthread_join(solver_tids[i], NULL);
        }
    }
    set_solve_complete_flag(args->solve_complete_flag_ptr, 1);
    pthread_join(reader_tid, NULL);
    pthread_join(writer_tid, NULL);

    /* Do cleanup */
    Queue_delete(args->input_queue);
    Queue_delete(args->output_queue);
    pthread_mutex_destroy(&read_complete_flag_lock);
    pthread_mutex_destroy(&solve_complete_flag_lock);
    free(args);
    fclose( inputfile );
    fclose( outputfile );
    return 0;
}

/* 
 * Function being run by reader thread that will stop when:
 * - no more puzzles left to be read from input file
 */ 
void *read_handler(void *args) {
    sudoku_workers_input *arguments = (sudoku_workers_input*) args;
    Queue* q_in = arguments->input_queue;
    puzzle *p;

    while ((p = read_next_puzzle(arguments->input_file)) != NULL) {
        Queue_add(q_in, (void*)p);
    }

    /* Signal to other threads that there are no more puzzles to be read in*/
    set_read_complete_flag(arguments->read_complete_flag_ptr, 1);
}

/* 
 * Function being run by solver thread.
 */
void *solve_handler(void *args) {
    sudoku_workers_input *arguments = (sudoku_workers_input*) args;
    Queue* q_in = arguments->input_queue;
    Queue* q_out = arguments->output_queue;

    puzzle *p = (puzzle*)Queue_remove(q_in);

    if (solve(p, 0, 0)) {
        Queue_add(q_out, (void*) p);
    } else {
        printf("Illegal sudoku (or a broken algorithm)\n");
    }
}

/* 
 * Function being run by the writer thread that will stop running 
 * when these conditions are fulfilled:
 * - no more puzzles left in the output queue
 * - reader thread is done processing the input file
 * - solver threads are done processing the input queue
 */
void *write_handler(void *args) {
    sudoku_workers_input *arguments = (sudoku_workers_input*) args;
    Queue* q_out = arguments->output_queue;
    Queue* q_in = arguments->input_queue;

    while (Queue_size(q_out) != 0 || read_read_complete_flag(arguments->read_complete_flag_ptr) == 0 || read_solve_complete_flag(arguments->solve_complete_flag_ptr) == 0) {
        if (Queue_size(q_out) != 0) {
            puzzle *p = (puzzle*)Queue_remove(q_out);
            write_to_file(p, arguments->output_file);
            free(p);
        }
    }
}

void set_solve_complete_flag(int *flag, int value) {
    pthread_mutex_lock(&solve_complete_flag_lock);
    *(flag) = value;
    pthread_mutex_unlock(&solve_complete_flag_lock);
}

int read_solve_complete_flag(int *flag) {
    pthread_mutex_lock(&solve_complete_flag_lock);
    int result = *(flag);
    pthread_mutex_unlock(&solve_complete_flag_lock);
    return result;
}

void set_read_complete_flag(int *flag, int value) {
    pthread_mutex_lock(&read_complete_flag_lock);
    *(flag) = value;
    pthread_mutex_unlock(&read_complete_flag_lock);
}

int read_read_complete_flag(int *flag) {
    pthread_mutex_lock(&read_complete_flag_lock);
    int result = *(flag);
    pthread_mutex_unlock(&read_complete_flag_lock);
    return result;
}

/*
 * A recursive function that does all the gruntwork in solving
 * the puzzle.
 */
int solve(puzzle *p, int row, int column) {
    int nextNumber = 1;
    /*
     * Have we advanced past the puzzle?  If so, hooray, all
     * previous cells have valid contents!  We're done!
     */
    if (9 == row) {
        return 1;
    }

    /*
     * Is this element already set?  If so, we don't want to
     * change it.
     */
    if (p->content[row][column]) {
        if (column == 8) {
            if (solve(p, row + 1, 0)) return 1;
        } else {
            if (solve(p, row, column + 1)) return 1;
        }
        return 0;
    }

    /*
     * Iterate through the possible numbers for this empty cell
     * and recurse for every valid one, to test if it's part
     * of the valid solution.
     */
    for (; nextNumber < 10; nextNumber++) {
        if (is_valid(nextNumber, p, row, column)) {
            p->content[row][column] = nextNumber;
            if (column == 8) {
                if (solve(p, row + 1, 0)) return 1;
            } else {
                if (solve(p, row, column + 1)) return 1;
            }
            p->content[row][column] = 0;
        }
    }
    return 0;
}

/*
 * Checks to see if a particular value is presently valid in a
 * given position.
 */
int is_valid(int number, puzzle *p, int row, int column) {
    int modRow = 3 * (row / 3);
    int modCol = 3 * (column / 3);
    int row1 = (row + 2) % 3;
    int row2 = (row + 4) % 3;
    int col1 = (column + 2) % 3;
    int col2 = (column + 4) % 3;

    /* Check for the value in the given row and column */
    for (int i = 0; i < 9; i++) {
        if (p->content[i][column] == number) return 0;
        if (p->content[row][i] == number) return 0;
    }

    /* Check the remaining four spaces in this sector */
    if (p->content[row1 + modRow][col1 + modCol] == number) return 0;
    if (p->content[row2 + modRow][col1 + modCol] == number) return 0;
    if (p->content[row1 + modRow][col2 + modCol] == number) return 0;
    if (p->content[row2 + modRow][col2 + modCol] == number) return 0;
    return 1;
}
