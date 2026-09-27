#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t joined_size(const char *const *parts, int n) {
    size_t total = 1;                       
    for (int i = 0; i < n; i++) {    
        printf("strlen(parts[%d]:%s,%zu \n",i,parts[i],strlen(parts[i]));
        total += strlen(parts[i]);
    }
    return total;
}

static char *join(const char *const *parts, int n) {
    size_t need = joined_size(parts, n); //28
    char *out = malloc(need);            
    if (!out) { perror("malloc"); exit(1); }

    size_t off = 0;
    for (int i = 0; i < n; i++) {           
        strcpy(out + off, parts[i]); //🚨
        printf("strlen(parts[%d]:%s,%zu \n",i,parts[i],strlen(parts[i]));
        off += strlen(parts[i]);
        printf("off:%d\n",off);
    }
    out[off] = '\0';
    return out;
}

int main(void) {
    
    static char body[200000];
    memset(body, 'x', sizeof body - 1);
    body[sizeof body - 1] = '\0';

    const char *parts[] = { "GET ", "/index.html", " HTTP/1.1\r\n\r\n", body };
    int n = (int)(sizeof(parts) / sizeof(parts[0])); // 32/8 //원소 개수 

    char *msg = join(parts, n); //🚨 

    printf("joined length = %zu\n", strlen(msg));
    free(msg);
    return 0;
}
