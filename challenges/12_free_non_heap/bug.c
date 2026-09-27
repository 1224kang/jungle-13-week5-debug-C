#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 8
typedef struct {
    char *base;                 
    char *fields[MAX_FIELDS];   
    int   n;
} Row;

static void parse_row(Row *r, const char *csv) {
    //strdup = 문자열을 힙에 복사해서 그 주소를 돌려줌 
    r->base = strdup(csv);       
    if (!r->base) { perror("strdup"); exit(1); }
    r->n = 0;

    //strtok = 구분자 기준으로 잘라서 토큰을 하나씩 꺼내줌 
    for (char *tok = strtok(r->base, ","); tok && r->n < MAX_FIELDS;
         tok = strtok(NULL, ",")) {
        r->fields[r->n++] = tok;  //n++:현재 n값을 인덱스로 쓰고 그 다음에 n을 1 늘림
    }
}

static void row_print(const Row *r) {
    printf("%d fields:", r->n);
    for (int i = 0; i < r->n; i++) printf(" [%s]", r->fields[i]);
    printf("\n");
}

static void row_free(Row *r) {
    // for (int i = 0; i < r->n; i++) {
    //     free(r->fields[i]);  //🚨    
    // }
    free(r->base);
    r->base=NULL;
    r->n = 0;
}

int main(void) {
    Row r;
    parse_row(&r, "id,name,dept,salary");
    row_print(&r);

    row_free(&r); //🚨                 
    printf("done\n");
    return 0;
}
