
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int   id;
    char *name;      
} Rec;

#define MAXN 16
typedef struct {
    Rec *by_id[MAXN];     
    Rec *by_name[MAXN];    
    int  count;
} Directory;

static Rec *rec_new(int id, const char *name) {
    Rec *r = malloc(sizeof *r); //8B
    if (!r) { perror("malloc"); exit(1); }
    r->id = id;
    r->name = malloc(strlen(name) + 1);
    if (!r->name) { perror("malloc"); exit(1); }
    strcpy(r->name, name);
    return r;
}

static void directory_add(Directory *d, int id, const char *name) {
    Rec *r = rec_new(id, name);
    d->by_id[d->count]   = r;
    d->by_name[d->count] = r;      /* 같은 포인터를 두 인덱스에 함께 등록 */
    d->count++;
}

/* 이름 순 인덱스를 사전순으로 정렬(포인터만 재배치, 객체는 공유 그대로) */
static void directory_sort_by_name(Directory *d) {
    for (int i = 0; i < d->count; i++) {
        for (int j = i + 1; j < d->count; j++) {
            //두 문자열을 사전순으로 비교 
            //양수 = s2이 s1보다 사전순으로 앞섬
            if (strcmp(d->by_name[i]->name, d->by_name[j]->name) > 0) { //더 앞선놈을 앞으로 보내도록 스위치 
                Rec *t = d->by_name[i];
                d->by_name[i] = d->by_name[j];
                d->by_name[j] = t;
            }
        }
    }
}

static Rec *find_by_id(Directory *d, int id) {
    for (int i = 0; i < d->count; i++)
        if (d->by_id[i]->id == id) return d->by_id[i];
    return NULL;
}

static void directory_dump(Directory *d) {
    printf("by id:  ");
    for (int i = 0; i < d->count; i++) printf("%d:%s ", d->by_id[i]->id, d->by_id[i]->name);
    printf("\nby name:");
    for (int i = 0; i < d->count; i++) printf(" %s(%d)", d->by_name[i]->name, d->by_name[i]->id);
    printf("\n");
}

static void directory_free(Directory *d) {
    for (int i = 0; i < d->count; i++) {
        free(d->by_id[i]->name);
        free(d->by_id[i]);                 
    }
    // for (int i = 0; i < d->count; i++) {
    //     free(d->by_name[i]);               
    // }
    d->count = 0;
}

int main(void) {
    Directory dir = { .count = 0 };

    directory_add(&dir, 3, "carol");
    directory_add(&dir, 1, "alice");
    directory_add(&dir, 4, "dave");
    directory_add(&dir, 2, "bob");

    directory_sort_by_name(&dir);
    directory_dump(&dir);

    Rec *r = find_by_id(&dir, 2);
    if (r) printf("lookup id=2 -> %s\n", r->name);

    directory_free(&dir);                  
    printf("done\n");
    return 0;
}
