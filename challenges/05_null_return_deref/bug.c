
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_KV 16
typedef struct {
    const char *keys[MAX_KV];
    const char *vals[MAX_KV];
    int n;
} Config;

static void cfg_set(Config *c, const char *k, const char *v) {
    if (c->n < MAX_KV) { c->keys[c->n] = k; c->vals[c->n] = v; c->n++; }
}

static const char *cfg_get(const Config *c, const char *k) {
    for (int i = 0; i < c->n; i++) //c가 n보다 작은 경우 계속 반복
        //사전순으로 비교
        if (strcmp(c->keys[i], k) == 0) return c->vals[i];
    return NULL;                       
}

static int expand(const Config *c, const char *tmpl, char *out, size_t outcap) {
    size_t o = 0;
    int truncated=0;

    for (const char *p = tmpl; *p; ) { //*p가 \0이 아닌 동안 계속 반복 
        if (p[0] == '$' && p[1] == '{') {
            //특정 문자가 처음 나타나는 위치를 찾음 
            //strchr(검색할 문자열,찾을 문자)
            const char *end = strchr(p, '}');
            if (!end) break;
            char key[32];
            //포인터 뺄셈의 결과타입은 부호 있는 정수이기 때문에 부호를 없애주기 위해 size_t 로 캐스팅 
            size_t kl = (size_t)(end - (p + 2)); //중괄호 안의 변수명 길이를 구함 //4
            //kl이 너무 길면 잘라버리기 
            if (kl >= sizeof key) kl = sizeof key - 1; 
            //memcpy(복사해 넣을 대상 주소,복사할 원본 주소,복사할 바이트 수)
            memcpy(key, p + 2, kl); //메모리 블록을 있는 그대로 복사
            key[kl] = '\0';

            const char *v = cfg_get(c, key);   
            if (v!=NULL){
                size_t vl = strlen(v);  
                if (o + vl < outcap) {
                    memcpy(out + o, v, vl+1); 
                    o += vl; 
                }
                //truncation 발생
                else{
                    truncated=1;
                }
            }  
            p = end + 1;
        } else {
            if (o + 1 < outcap) out[o++] = *p;
            else truncated=1;
            p++;
        }
    }
    out[o] = '\0';

    return truncated?-1:0;
}

int main(void) {

    Config cfg = { .n = 0 };
    cfg_set(&cfg, "host", "example.com");
    cfg_set(&cfg, "port", "8080");


    const char *tmpl = "http://${host}:${port}/${path}/index.html";
    // const char *tmpl="http://${host}/";
    char out[256];

    // expand(&cfg, tmpl, out, sizeof out);   
    if(expand(&cfg,tmpl,out,sizeof out)!=0){
        fprintf(stderr,"경고: 출력이 버퍼 크기를 초과해서 잘렸습니다.\n");
    }

    printf("url = %s\n", out);
    return 0;
}
