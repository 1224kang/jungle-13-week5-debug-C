
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Widget Widget;

typedef struct {
    void (*render)(Widget *self); //Widget * 타입의 인자 한개 받고 반환값 없는 함수 포인터 
    void (*on_event)(Widget *self, int code);
} VTable;

//8+4+4+24=40B
struct Widget {
    const VTable *vtbl; //8B
    int id; //4B
    int closed; //4B
    char label[24]; //24B
};

#define MAX_WIDGETS 8
typedef struct {
    Widget *items[MAX_WIDGETS];
    int count;
} Screen;

/* ── 위젯 종류별 동작 ─────────────────────────────────────────── */
static void button_render(Widget *self) {
    printf("  [Button #%d] \"%s\"\n", self->id, self->label);
}
static void label_render(Widget *self) {
    printf("  Label #%d: %s\n", self->id, self->label);
}
static void dialog_render(Widget *self) {
    printf("  <<Dialog #%d>> %s\n", self->id, self->label);
}

//아무 작업도 하지 않는 이벤트 
static void widget_noop_event(Widget *self, int code) { (void)self; (void)code; }

/* 다이얼로그는 이벤트 코드 1(닫기)을 받으면 스스로 정리(파괴)된다 */
static void dialog_on_event(Widget *self, int code);

static const VTable BUTTON_VT = { button_render, widget_noop_event };
static const VTable LABEL_VT  = { label_render,  widget_noop_event };
static const VTable DIALOG_VT = { dialog_render, dialog_on_event  };

static Widget *widget_new(const VTable *vt, int id, const char *label) {

    Widget *w = malloc(sizeof *w);
    if (!w) { perror("malloc"); exit(1); }
    w->vtbl = vt;
    w->id = id;
    w->closed = 0;
    //문자열 복사 (*destination,*source,size(bytes))
    strncpy(w->label, label, sizeof(w->label) - 1);
    w->label[sizeof(w->label) - 1] = '\0';
    return w;
}

static void widget_destroy(Widget *w) {
    free(w);  
}

/* ── Screen ──────────────────────────────────────────────────── */
static void screen_add(Screen *s, Widget *w) {
    if (s->count < MAX_WIDGETS) s->items[s->count++] = w;
}

static void screen_dispatch(Screen *s, int code) {
    for (int i = 0; i < s->count; i++) {
        Widget *w = s->items[i];
        w->vtbl->on_event(w, code); //on_event는 이제 Free를 하지않음 

        if(w->closed!=0){
            widget_destroy(w); //Free 전이므로 안전하게 읽을 수 있음 
            
            for(int j=i;j<s->count-1;j++){
                s->items[j]=s->items[j+1];
            }
            s->items[s->count-1]=NULL;
            s->count-=1;

            i--; //당겨진 새로운 원소. 새로운 원소도 아직 검사를 안했으므로 다시 검사해야 함 
        }
    }
    
}

static void screen_render(Screen *s) {
    printf("s size:%d \n",s->count);
    for (int i = 0; i < s->count; i++) {
        Widget *w = s->items[i];
        if (w==NULL){
            continue;
        }
        w->vtbl->render(w); //vtbl에서 render라는 포인터를 가져오기 
    }
}

static void dialog_on_event(Widget *self, int code) {
    if (code == 1) {
        self->closed = 1;
    }
}

static char *app_build_status(const char *text) {
    char *msg = malloc(sizeof(Widget));   
    if (!msg) exit(1);

    memset(msg, 0xAB, sizeof(Widget)); //메모리 특정 영역을 지정한 값으로 채우는 함수 
    printf("sizeOf(Widget):%ld \n",sizeof(Widget));
    //23바이트만 사용
    snprintf(msg, sizeof(Widget), "STATUS: %s", text); //문자열을 원하는 형식으로 만들어서 문자 배열에 저장 
    return msg;
}

int main(void) {
    Screen s = { .count = 0 };

    screen_add(&s, widget_new(&LABEL_VT,  10, "Welcome"));
    screen_add(&s, widget_new(&BUTTON_VT, 11, "OK"));
    screen_add(&s, widget_new(&DIALOG_VT, 12, "Are you sure?"));  /* items[2] */
    screen_add(&s, widget_new(&BUTTON_VT, 13, "Cancel"));

    printf("frame 1:\n");
    screen_render(&s);
    screen_dispatch(&s, 1);

    char *status = app_build_status("dialog closed");
    printf("%s\n", status);

    printf("frame 2:\n");
    screen_render(&s);           

    free(status);
    for (int i = 0; i < s.count; i++) free(s.items[i]);
    return 0;
}
