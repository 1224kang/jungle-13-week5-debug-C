#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//PermFn:const char*를 받아 int를 반환하는 함수를 가리키는 포인터 
typedef int (*PermFn)(const char *action);

typedef struct {
    PermFn permission;      
    int    uid;
    char   name[24];
} User;

typedef struct {
    User *user;             
    int   session_id;
} Session;

static int allow_all(const char *action) { (void)action; return 1; }

static User *login(int uid, const char *name) {
    User *u = malloc(sizeof *u);
    if (!u) { perror("malloc"); exit(1); }
    u->permission = allow_all;
    u->uid = uid;
    strncpy(u->name, name, sizeof(u->name) - 1);
    u->name[sizeof(u->name) - 1] = '\0';
    return u;
}

static void logout(Session *s) {
    free(s->user);      
    s->user=NULL;   
}


static char *audit_record(const char *event) {
    char *rec = malloc(sizeof(User));       
    if (!rec) exit(1);
    memset(rec, 0xAB, sizeof(User));        
    snprintf(rec, sizeof(User), "audit:%s", event);
    return rec;
}

static int handle_request(Session *s, const char *action) {
    if(s->user!=NULL){
        return s->user->permission(action);   //🚨
    }
    else{
        return 0;
    }
    
}

int main(void) {
    Session s;
    s.session_id = 1;
    s.user = login(42, "alice");

    printf("first request allowed=%d\n", handle_request(&s, "read"));

    logout(&s);                              

    char *rec = audit_record("logout");      
    printf("%s\n", rec);
    
    printf("second request allowed=%d\n", handle_request(&s, "write")); //🚨

    free(rec);
    return 0;
}
