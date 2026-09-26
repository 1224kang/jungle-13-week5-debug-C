
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HEADERS 32
typedef struct {
    char *keys[MAX_HEADERS]; //배열의 각 원소->char*(문자를 가리키는 포인터)
    char *vals[MAX_HEADERS];
    int   count;
} Headers;

static char *skip_ws(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static void parse_headers(char *text, Headers *h) {
    //strtok=문자열을 구분자 기준으로 토큰으로 잘라냄 
    for (char *line = strtok(text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        
        //문자열 line에서 문자 ':'가 처음 나타나는 위치를 찾아 그 지점의 포인터 반환 
        char *colon = strchr(line, ':');   
        char *val;

        if (colon!=NULL){
            *colon = '\0';
            val = skip_ws(colon + 1);
        }else{
            val=NULL;
        }
                 
        char *key = line;
        

        if (h->count < MAX_HEADERS) {
            h->keys[h->count] = key;
            h->vals[h->count] = val;
            h->count++;
        }
    }
}

int main(void) {

    char raw[] =
        "Host: example.com\n"
        "Accept: */*\n"
        "Connection\n"                     
        "User-Agent: memdbg-cli\n";

    Headers h = { .count = 0 };
    parse_headers(raw, &h);                

    printf("parsed %d headers\n", h.count);
    for (int i = 0; i < h.count; i++)
        printf("  %s = %s\n", h.keys[i], h.vals[i]);
    return 0;
}
