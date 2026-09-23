#include <stdio.h>
#include <stdlib.h>

#define ROWS 3
enum { SIZE = (ROWS * (ROWS + 1) / 2) };   /* 0..ROWS-1 행을 담는 정확한 크기 */

static int tri_index(int i, int j) {
    return i * (i + 1) / 2 + j;
}

static void build_pascal(int *tri, int rows) {
    for (int i = 0; i < rows; i++) {
        printf("i:%d   ",i);
        for (int j = 0; j <= i; j++) {
            int idx = tri_index(i, j);
            if (j == 0 || j == i) {
                tri[idx] = 1;                        
            } else {
                int up_left  = tri_index(i - 1, j - 1);
                int up_right = tri_index(i - 1, j);
                tri[idx] = tri[up_left] + tri[up_right];
            }
            printf("tri[%d]:%d, ",idx,tri[idx]);
        }
        printf("\n");
    }
}

static long row_sum(const int *tri, int i) {
    long sum = 0;
    for (int j = 0; j <= i; j++) sum += tri[tri_index(i, j)];
    return sum;
}

static void print_row(const int *tri, int i) {
    printf("row %2d:", i);
    for (int j = 0; j <= i; j++) printf(" %d", tri[tri_index(i, j)]);
    printf("   (sum=%ld)\n", row_sum(tri, i));
}

int main(void) {
    int tri[SIZE]; //tri[105]

    build_pascal(tri, ROWS);          

    for (int i = 0; i < ROWS; i++) print_row(tri, i);

    printf("SIZE = %d\n", SIZE);


    return 0;
}
