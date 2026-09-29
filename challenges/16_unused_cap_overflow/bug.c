#include <stdio.h>
#include <string.h>


static int append_field(char *buf, size_t cap, size_t *len, const char *field, char sep) {
    
    size_t flen = strlen(field); //7

    if ((*len+flen+1)>cap){
        return -1;
    }

    if (*len > 0) {
        buf[(*len)++] = sep;             
    }

    for (size_t i = 0; i < flen; i++) {
        buf[(*len)++] = field[i];         
    }
    buf[*len] = '\0';
    (void)cap;                  
    return 0;
         
}

static void build_record(char *rec, size_t cap) {
    const char *fields[] = {
        "id=1042", "name=Jonathan", "department=Engineering", "role=maintainer",
    };
    int n = (int)(sizeof(fields) / sizeof(fields[0]));

    size_t len = 0;
    rec[0] = '\0';
    for (int i = 0; i < n; i++) {
        int result=append_field(rec, cap, &len, fields[i], '|');   
        if(result==-1){
            return;
        }
    }
}

int main(void) {
    char rec[24];                         

    build_record(rec, sizeof rec);        

    printf("record = %s\n", rec);
    return 0;                            
} //🚨
