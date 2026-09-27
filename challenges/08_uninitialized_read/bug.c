
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS 32
#define COLS 4

static void dirty_heap(void) {
    void *scratch = malloc(ROWS * sizeof(int *));
    if (scratch) {
        //memset:메모리 블록의 모든 바이트를 특정 값으로 채움 
        //(채울 대상 메모리의 시작 주소,채울값,채울 바이트수)
        memset(scratch, 0xAB, ROWS * sizeof(int *));
        free(scratch);              
    }
}

static int **make_matrix(void) {

    int **rows = malloc(ROWS * sizeof(int *));
    if (!rows) { perror("malloc"); exit(1); }

    for (int i = 0; i < ROWS; i ++) {
        int *r = malloc(COLS * sizeof(int));
        for (int j = 0; j < COLS; j++) r[j] = i * COLS + j;
        rows[i] = r;
    }
    return rows;
}

static long row_sum(int **rows, int nrows) {
    long total = 0;
    for (int i = 0; i < nrows; i++) {//32
        for (int j = 0; j < COLS; j++) {//4
            total += rows[i][j];  //🚨
        }
    }
    return total;
}

int main(void) {
    dirty_heap();

    int **rows = make_matrix();
    printf("summing %dx%d matrix...\n", ROWS, COLS);

    long s = row_sum(rows, ROWS); //🚨

    printf("sum = %ld\n", s);

    for (int i = 0; i < ROWS; i ++) free(rows[i]);
    free(rows);
    return 0;
}
