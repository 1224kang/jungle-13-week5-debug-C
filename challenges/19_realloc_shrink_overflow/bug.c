#include <stdio.h>
#include <stdlib.h>

typedef struct {
    double *samples;
    size_t  len;      
    size_t  cap;      
} Signal;

static void signal_init(Signal *s, size_t n) {
    s->samples = malloc(n * sizeof(double));
    if (!s->samples) { perror("malloc"); exit(1); }
    s->len = s->cap = n;
    for (size_t i = 0; i < n; i++) s->samples[i] = (double)(i % 7) - 3.0;
}

static void signal_trim(Signal *s, size_t keep) {
    if (keep > s->cap) return;
    double *p = realloc(s->samples, keep * sizeof(double));
    if (p) s->samples = p; //⭐️
    s->cap = keep;                
    s->len=keep; 
}

static double signal_energy(const Signal *s) {
    double e = 0.0;
    for (size_t i = 0; i < s->len; i++) {   
        e += s->samples[i] * s->samples[i]; //🚨
    }
    return e;
}


int main(void) {
    Signal s;
    signal_init(&s, 2000000);       

    signal_trim(&s, 8);             

    double e = signal_energy(&s); //🚨  
    
    printf("energy = %.1f (len=%zu cap=%zu)\n", e, s.len, s.cap);
    free(s.samples);
    return 0;
}
