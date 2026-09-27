#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_SIZE 4096

static unsigned char arena[ARENA_SIZE];   
static size_t arena_off = 0;

static void *arena_alloc(size_t n) { //9
    if(arena_off+n>ARENA_SIZE){
        return NULL;
    }
    void *p = &arena[arena_off];
    arena_off +=n;
    return p;
}

static char *intern(const char *s) {
    size_t n = strlen(s)+1; //char * -> size=8B
    char *dst = arena_alloc(n);
    if(dst!=NULL){
        memcpy(dst, s, n);  //🚨              
    }
    return dst;
    
}

int main(void) {
    
    const char *words[] = {
        "insert", "delete", "search", "traverse", "balance",
        "rotate", "rehash", "compact", "serialize", "checkpoint",
    };
    int nwords = (int)(sizeof(words) / sizeof(words[0])); //10

    char *last = NULL;
    long total = 0;
    for (int i = 0; i < 100000; i++) {
        char buf[32];
        snprintf(buf, sizeof buf, "%s-%d", words[i % nwords], i);
        printf("i:%d\n",i);
        last = intern(buf); //🚨      
        if (last==NULL){
            break;
        }         
        total += (long)strlen(last);
        printf("total:%ld\n",total);
    }

    printf("interned, last=%s total_len=%ld\n", last, total);
    return 0;
}
