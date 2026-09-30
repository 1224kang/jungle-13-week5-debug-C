#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int  key;
    long count;
} Bucket;

typedef struct {
    Bucket *data;
    size_t  len, cap;
} Histogram;

static void hist_grow(Histogram *h) {
    h->cap = h->cap ? h->cap * 2 : 16;
    Bucket *p = realloc(h->data, h->cap * sizeof(Bucket));   /* 큰 배열은 이동(mmap 재배치) */
    if (!p) { perror("realloc"); free(h->data); exit(1); }
    h->data = p; //배열이 옮겨졌을 수 있으므로 새 주소로 갱신 
}

/* 키를 추가하고, 그 버킷의 주소를 돌려준다(성장이 일어날 수 있음). */
static Bucket *hist_add(Histogram *h, int key) {
    if (h->len == h->cap) hist_grow(h);
    Bucket *b = &h->data[h->len++]; //=&(h->data[h->len++]) //해당 칸의 주소
    b->key = key;
    b->count = 0;
    return b;
}

static long hist_total(const Histogram *h) {
    long t = 0;
    for (size_t i = 0; i < h->len; i++) t += h->data[i].count;
    return t;
}

int main(void) {
    Histogram h = { .data = NULL, .len = 0, .cap = 0 };

    for (int k = 0; k < 200000; k++) hist_add(&h, k);

    size_t hot_index=100000; //인덱스 방식으로 수정
    h.data[hot_index].count=1;

    for (int k = 200000; k < 600000; k++) hist_add(&h, k);
    
    h.data[hot_index].count+=1000;

    printf("hot=%ld total=%ld len=%zu\n", h.data[hot_index].count, hist_total(&h), h.len);
    free(h.data);
    return 0;
}
