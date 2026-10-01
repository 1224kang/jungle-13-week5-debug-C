
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 8
typedef struct {
    char **lines;   
    int    count;
} LineView;


static void view_set(LineView *out,  int n) {
    // out->lines = arr;
    out->count = n;
}

static void split_lines(LineView *out, char *text) {
    // char * parts[MAX_LINES]; //8        
    // char **parts = malloc(sizeof(char*)* MAX_LINES); //수정2           
    int n = 0;
    for (char *ln = strtok(text, "\n"); ln && n < MAX_LINES; ln = strtok(NULL, "\n"))
        // parts[n++] = ln; //수정2
        out->lines[n++]=ln; //수정1
        

    view_set(out,  n);      

}


static void warm_stack(void) {
    char *scratch[MAX_LINES];
    for (int i = 0; i < MAX_LINES; i++)
        scratch[i] = (char *)0x4141414141414141ULL;
    __asm__ volatile("" :: "r"(scratch) : "memory");  //컴파일러 최적화를 막기 위함 
}


int main(void) {
    char text[] = "alpha\nbeta\ngamma";
    char *parts[MAX_LINES];//수정 1

    LineView v;
    v.lines=parts; //수정1
    
    split_lines(&v, text);  
    warm_stack();              
                           

    long checksum = 0;
    for (int i = 0; i < v.count; i++)
        checksum += (unsigned char)v.lines[i][0]; //🚨

    printf("lines = %d, checksum = %ld\n", v.count, checksum);
    // free(v.lines);//수정2
    return 0;
}
