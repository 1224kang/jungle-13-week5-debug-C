#include <stdio.h>
#include <stdlib.h>

typedef struct Job {
    int id;
    int priority;
    struct Job *next;
} Job;

typedef struct {
    int   *ids;
    size_t len, cap;
} Audit;

static void audit_add(Audit *a, int id) {
    if (a->len == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 16; //0이 아니면 참 
        int *p = realloc(a->ids, a->cap * sizeof(int));   
        if (!p) { perror("realloc"); exit(1); }
        a->ids = p;
    }
    a->ids[a->len++] = id;
}

static Job *push_job(Job *head, int id, int priority) {
    Job *n = malloc(sizeof *n);
    if (!n) { perror("malloc"); exit(1); }
    n->id = id;
    n->priority = priority;
    n->next = head;
    return n;
}


static void job_release(Job *j) {
    free(j);
}

static Job *filter_jobs(Job *head, int threshold, Audit *audit) {
    Job *keep = NULL, *keep_tail = NULL;
    Job *cur = head;
    Job *nx=NULL;

    while (cur != NULL) {
        if (cur->priority < threshold) { //🚨
            audit_add(audit, cur->id);    
            nx=cur->next;                    
            job_release(cur);   
            cur = nx;
        } else {
            nx=cur->next;
            cur->next = NULL;
            if (keep_tail) keep_tail->next = cur; else keep = cur;
            keep_tail = cur;
            cur = nx;
        }
    }
    return keep;
}

int main(void) {
    Job *head = NULL;
    for (int i = 1; i <= 4000; i++)
        head = push_job(head, i, (i * 7) % 10);   

    Audit audit = {0}; //제로 초기화:구조체의 모든 필드를 0으로 초기화
    head = filter_jobs(head, 5, &audit); //🚨        

    int remaining = 0;
    for (Job *c = head; c; c = c->next) remaining++;
    printf("cancelled=%zu remaining=%d\n", audit.len, remaining);

    free(audit.ids);
    for (Job *c = head; c; ) { Job *nx = c->next; free(c); c = nx; }
    return 0;
}
