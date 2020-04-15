// from the Cilk manual: http://supertech.csail.mit.edu/cilk/manual-5.4.6.pdf
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECURSIVE_DEPTH 1

/* 
 * Checks whether queen position at column j conflicts
 * with queens already set on the board
 */
int safe(char * config, int i, int j)
{
    int r, s;

    for (r = 0; r < i; r++)
    {
        s = config[r];
        if (j == s || i-r==j-s || i-r==s-j)
            return 0;
    }
    return 1;
}

int count = 0;

void nqueens(char *config, int n, int i)
{
    /* increment when all queens have been placed */
    if (i==n)
    {
        #pragma omp atomic
        count++;
    }
    
    if (i < RECURSIVE_DEPTH) {
        for (int j = 0; j < n; j++) {
            if (safe(config, i, j)) {
                #pragma omp task firstprivate(j)
                {
                    char *new_config = malloc((i+1)*sizeof(char));
                    memcpy(new_config, config, i*sizeof(char));
                    new_config[i] = j;
                    nqueens(new_config, n, i+1);
                    free(new_config);
                } 
            }
        }
    }
    else {
        /* allocate a temporary array and copy the config into it */
        char *new_config = malloc((i+1)*sizeof(char));
        memcpy(new_config, config, i*sizeof(char));
        /* try each possible position for queen <i> */
        for (int j = 0; j < n; j++) {
            if (safe(new_config, i, j))
            {
                new_config[i] = j;
                nqueens(new_config, n, i+1);
            }
        }
        free(new_config);
    }

    return;
}

int main(int argc, char *argv[])
{
    int n;
    char *config;

    if (argc < 2)
    {
        printf("%s: number of queens required\n", argv[0]);
        return 1;
    }

    n = atoi(argv[1]);
    config = malloc(n * sizeof(char));

    printf("running queens %d\n", n);
    #pragma omp parallel 
    {
        #pragma omp single 
        {
            nqueens(config, n, 0);
        }
    }
    printf("# solutions: %d\n", count);
    free( config );
    return 0;
}
