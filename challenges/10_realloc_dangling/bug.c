#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_UNDO 8
typedef struct {
    int   *data;
    size_t len, cap;
    int   *clipboard;       
    int   *undo[MAX_UNDO];   
    int    undo_n;
} EditBuffer;

static void eb_init(EditBuffer *e) {
    e->cap = 4;
    e->len = 0;
    e->undo_n = 0;
    e->data = malloc(e->cap * sizeof(int)); //16byte
    if (!e->data) { perror("malloc"); exit(1); }
    e->clipboard = malloc(e->cap * sizeof(int));
    if (!e->clipboard) { perror("malloc"); exit(1); }
}

static void eb_snapshot(EditBuffer *e) {
    if (e->undo_n < MAX_UNDO){
        int *copy=malloc(e->len*sizeof(int));
        if (!copy) { perror("malloc"); exit(1); }
        memcpy(copy,e->data,e->len*sizeof(int));
        e->undo[e->undo_n++] = copy;
    } 
}

static void eb_grow(EditBuffer *e, size_t need) {
    size_t nc = e->cap;
    while (nc < need) nc *= 2;
    int *p = realloc(e->data, nc * sizeof(int));   
    if (!p) { perror("realloc"); free(e->data); exit(1); }
    e->data = p;                                   
    e->cap = nc;
}

static void eb_push(EditBuffer *e, int v) {
    if (e->len == e->cap) eb_grow(e, e->len + 1);
    e->data[e->len++] = v;
}

static void eb_free(EditBuffer *e) {
    free(e->data);
    

    free(e->clipboard);
    for (int i = 0; i < e->undo_n; i++) {
        free(e->undo[i]); 
        e->undo[i]=NULL;        
    }
    e->undo_n = 0;
    e->data = NULL;
}

int main(void) {
    EditBuffer e;
    eb_init(&e);

    for (int i = 0; i < 3; i++) eb_push(&e, i);

    eb_snapshot(&e);                 

    for (int i = 0; i < 4000; i++) eb_push(&e, i);     

    printf("len=%zu cap=%zu head=%d tail=%d\n",
           e.len, e.cap, e.data[0], e.data[e.len - 1]);

    eb_free(&e); //🚨                   
    printf("done\n");
    return 0;
}
