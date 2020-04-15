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

location get_next_empty_space(puzzle *p, int row, int column);

void *solve_handler(void *args);

void set_result_found_flag(int *flag, int value);

int read_result_found_flag(int *flag);

void set_no_more_puzzles_flag(int *flag, int value);

int read_no_more_puzzles_flag(int *flag);

pthread_mutex_t result_found_flag_lock;

pthread_mutex_t no_more_puzzles_flag_lock;

pthread_mutex_t queue_access_lock;

int main(int argc, char **argv) {
    FILE *inputfile;
    FILE *outputfile;
    puzzle *p;

    /* Parse arguments */
    int c;
    int num_threads = 1;
    char *filename = NULL;
    while ((c = getopt(argc, argv, "t:i:")) != -1) {
        switch (c) {
            case 't':
                num_threads = strtoul(optarg, NULL, 10);
                if (num_threads <= 0 || num_threads > 81) {
                    printf("%s: option requires an argument > 0 and <= 81 -- 't'\n", argv[0]);
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

    /* Init synchronization tools */
    pthread_mutex_init(&result_found_flag_lock, NULL);
    pthread_mutex_init(&queue_access_lock, NULL);
    init_locks(); /* Used for thread-safe write to file*/

    /* Create threads to solve the modified puzzles */
    pthread_t tids[num_threads];
    int result_found_flag = 0;
    int *result_found_flag_ptr = &result_found_flag;
    int no_more_puzzles_flag = 0;
    int *no_more_puzzles_flag_ptr = &no_more_puzzles_flag;
    Queue *modified_puzzles = Queue_init();

    /* Spawn threads */
    for (int i = 0; i < num_threads; i++) {
        sudoku_multi_input* args = (sudoku_multi_input*) malloc(sizeof(sudoku_multi_input));
        args->output_file = outputfile;
        args->puzzle_queue = modified_puzzles;
        args->result_found_flag_ptr = result_found_flag_ptr;
        args->no_more_puzzles_flag_ptr = no_more_puzzles_flag_ptr;
        pthread_create(&tids[i], NULL, solve_handler, (void*) args);
    }

    /* Main loop - solve puzzle, write to file.
     * The read_next_puzzle function is defined in the common header */
    while ((p = read_next_puzzle(inputfile)) != NULL) {
        location loc_1 = get_next_empty_space(p, 0, 0);
        location loc_2 = get_next_empty_space(p, loc_1.row, loc_1.column + 1);

        /* Create modified puzzles based on location of empty space(s)*/
        for (int i = 1; i <= 9; i++) {
            for (int j = 1; j <= 9; j++) {
                if (is_valid(i, p, loc_1.row, loc_1.column)) {
                    puzzle *modified_p = (puzzle*) malloc(sizeof(puzzle));
                    *modified_p = *p;
                    modified_p->content[loc_1.row][loc_1.column] = i;
                    if (is_valid(j, modified_p, loc_2.row, loc_2.column)) {
                        modified_p->content[loc_2.row][loc_2.column] = j;
                        Queue_add(modified_puzzles, modified_p);
                    }
                    else {
                        free(modified_p);
                    }
                }
            }
        }

        set_result_found_flag(result_found_flag_ptr, 0);

        while (!read_result_found_flag(result_found_flag_ptr)) {}

        // Flush unused puzzles from the queue
        pthread_mutex_lock(&queue_access_lock);
        while(Queue_size(modified_puzzles) > 0) {
            puzzle *tmp = Queue_remove(modified_puzzles);
            free(tmp);
        }
        pthread_mutex_unlock(&queue_access_lock);

        free(p);
    }

    /* Signal threads to finish */
    set_no_more_puzzles_flag(no_more_puzzles_flag_ptr, 1);

    /* Wait for threads to finish */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(tids[i], NULL);
    }

    /* Cleanup */
    pthread_mutex_destroy(&result_found_flag_lock);
    pthread_mutex_destroy(&queue_access_lock);
    Queue_delete(modified_puzzles);
    fclose( inputfile );
    fclose( outputfile );
    return 0;
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

/* 
 * Helper function to find the next empty space in a sudoku
 * puzzle given a starting point
 */
location get_next_empty_space(puzzle *p, int row, int column) {
    if (9 == row) {
        location result = {row - 1, column};
        return result;
    }

    if (p->content[row][column] == 0) {
        location result = {row, column};
        return result;
    }
    else {
        if (column >= 8) {
            return get_next_empty_space(p, row + 1, 0);
        } else {
            return get_next_empty_space(p, row, column + 1);
        }
    }
}

/*
 * Wrapper function for `solve` used for pthreads
 */
void *solve_handler(void *args) {
    sudoku_multi_input *arguments = (sudoku_multi_input*) args;

    while (!read_no_more_puzzles_flag(arguments->no_more_puzzles_flag_ptr)) {
        int result_found_flag = read_result_found_flag(arguments->result_found_flag_ptr);
        int condition = !result_found_flag;

        while (condition) {
            if (!result_found_flag) {
                /* START: critical section*/
                pthread_mutex_lock(&queue_access_lock);
                puzzle *p;
                int queue_size = Queue_size(arguments->puzzle_queue);
                if (queue_size > 0) {
                    p = Queue_remove(arguments->puzzle_queue);
                }
                pthread_mutex_unlock(&queue_access_lock); 
                /* END: critical section*/

                if (queue_size > 0) {
                    if (solve(p, 0, 0)) {
                        write_to_file_with_lock(p, arguments->output_file);
                        set_result_found_flag(arguments->result_found_flag_ptr, 1);
                    }
                    free(p);
                }
                result_found_flag = read_result_found_flag(arguments->result_found_flag_ptr); 
                condition = (queue_size > 0 && !result_found_flag);
            }
        }
    }
    free(arguments);
}

void set_result_found_flag(int *flag, int value) {
    pthread_mutex_lock(&result_found_flag_lock);
    *(flag) = value;
    pthread_mutex_unlock(&result_found_flag_lock);
}

int read_result_found_flag(int *flag) {
    pthread_mutex_lock(&result_found_flag_lock);
    int result = *(flag);
    pthread_mutex_unlock(&result_found_flag_lock);
    return result;
}

void set_no_more_puzzles_flag(int *flag, int value) {
    pthread_mutex_lock(&no_more_puzzles_flag_lock);
    *(flag) = value;
    pthread_mutex_unlock(&no_more_puzzles_flag_lock);
}

int read_no_more_puzzles_flag(int *flag) {
    pthread_mutex_lock(&no_more_puzzles_flag_lock);
    int result = *(flag);
    pthread_mutex_unlock(&no_more_puzzles_flag_lock);
    return result;
}
