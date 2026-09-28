#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> // 수정할 때 SIZE_MAX 로 곱셈 오버플로를 검사하라고 미리 넣어 둔 헤더

typedef struct {
    int width;
    int height;
    int channels;
    size_t nbytes; //데이터타입도 더 큰걸로 변경             
    unsigned char *px;
} Image;

static Image *image_new(int width, int height, int channels) {
    Image *img = malloc(sizeof *img);
    if (!img) { perror("malloc"); exit(1); }

    img->width = width;
    img->height = height;
    img->channels = channels;

    printf("max:%zu",SIZE_MAX);
    
    //오버플로우 검사
    if((size_t)width>SIZE_MAX/(size_t)height){
        fprintf(stderr,"곱셈 오버플로우 발생");
        exit(1);
    }

    size_t wh=(size_t)width*(size_t)height;

    if(wh>SIZE_MAX/(size_t)channels){
        fprintf(stderr,"곱셈 오버플로우 발생");
        exit(1);
    }
    
    img->nbytes=wh*channels;

    img->px = malloc((size_t)img->nbytes);     
    if (!img->px) { perror("malloc px"); exit(1); }
    return img;
}

static void image_fill(Image *img, unsigned char value) {

    size_t total = (size_t)img->width * (size_t)img->height * (size_t)img->channels;
    for (size_t i = 0; i < total; i++) {
        img->px[i] = value;  //🚨 
        // printf("i:%d\n",i);                  
    }
}

int main(void) {
    Image *img = image_new(655, 655, 4);
    printf("allocated nbytes(int)=%zu for %dx%d x%d\n",
           img->nbytes, img->width, img->height, img->channels);

    image_fill(img, 0xFF); //🚨                       

    printf("px[0]=%u\n", img->px[0]);
    free(img->px);
    free(img);
    return 0;
}
