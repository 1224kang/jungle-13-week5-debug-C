# 14. Integer Overflow in Allocation

`malloc`/`realloc`에 넘길 크기를 곱셈으로 계산할 때, 그 계산이 정수 타입의 최대값을 넘어버리면(오버플로) **실제로 필요한 것보다 훨씬 작은 크기가 할당**되어 힙 버퍼 오버플로로 이어지는 버그를 디버깅하고 수정한 기록.

## 1. 문제 원인

프로그램이 `SIGSEGV`로 크래시했다.

```
allocated nbytes(int)=0 for 65536x65536 x4

Program received signal SIGSEGV, Segmentation fault.
0x0000aaaaaaaa09bc in image_fill (img=0xaaaaaaac12a0, value=255 '\377')
    at challenges/14_integer_overflow_alloc/bug.c:30
30              img->px[i] = value;
```

크래시 출력에서 이상한 점이 바로 보인다. `65536 × 65536 × 4`를 계산하면 약 171억(17,179,869,184)이어야 하는데, **`nbytes`가 `0`으로 저장**되어 있다. 곱셈 결과가 제대로 담기지 않은 것이다.

<br/><br/>

## 2. 원인 분석

문제가 된 코드는 다음과 같다.

```c
typedef struct {
    int width;
    int height;
    int channels;
    int nbytes;              // 🚨 int
    unsigned char *px;
} Image;

img->nbytes = width * height * channels;   // 65536*65536*4 = 17,179,869,184
img->px = malloc((size_t)img->nbytes);
```

`nbytes`가 `int`(32비트, 부호 있음)라는 것이 문제의 핵심이다. `int`의 범위는 다음과 같다.

```
INT_MIN  =  -2,147,483,648   (−2^31)
INT_MAX  =   2,147,483,647   ( 2^31 − 1)
```

계산하려는 값(약 171억)은 `INT_MAX`(약 21억)를 한참 넘는다. 게다가 곱셈에 참여하는 `width`, `height`, `channels`가 전부 `int`라서 곱셈 자체가 `int`로 수행되고, 그 결과가 `int` 범위를 넘어 오버플로가 일어난다. 하필 `65536 × 65536 = 2^32`이라 32비트에서 정확히 wrap-around되어 `0`이 되고, 여기에 `4`를 곱해도 `0`이 된다.

그 결과:

```c
img->nbytes = 0;              // 오버플로로 0
img->px = malloc(0);          // 0바이트(또는 아주 작게) 할당
```

그런데 `image_fill`은 크기를 `size_t`로 **제대로** 계산해서 그만큼 쓰려고 한다.

```c
size_t total = (size_t)img->width * (size_t)img->height * (size_t)img->channels;
for (size_t i = 0; i < total; i++)
    img->px[i] = value;      // 🚨 할당한 것보다 훨씬 많이 씀 → 힙 오버플로
```

즉 **할당 크기 계산(int, 오버플로되어 0)** 과 **사용 크기 계산(size_t, 정확히 171억)** 이 어긋나면서, 거의 0바이트짜리 버퍼에 171억 바이트를 쓰려다 크래시가 난 것이다.

### 참고: `unsigned char *px`인 이유

픽셀 버퍼의 타입이 `unsigned char`인 것도 짚어둘 만하다. `unsigned char`는 메모리를 "글자"가 아니라 **원시 바이트(0~255)** 로 다룰 때 쓰는 타입이다. 같은 1바이트라도 `signed char`로 해석하면 128~255 구간이 음수로 뒤집히는데(예: `0xFF` → −1), 이미지·네트워크·이진 데이터에는 이 구간 값이 당연히 등장하므로 `unsigned char`로 다뤄야 안전하다.

<br/><br/>

## 시도했지만 틀린 접근들

### 1. `int` → `long long`으로 타입만 키우기

담는 그릇을 키우면 될 것 같지만, 이건 **오버플로가 일어나는 한계값만 뒤로 미룰 뿐** 구조적 문제를 해결하지 못한다. `long long`도 최대값이 있어 더 큰 입력에는 똑같이 터진다. 게다가 `long long`은 부호 있는 타입이라 오버플로가 **UB(undefined behavior)** 라서 오히려 더 위험하다. (부호 없는 타입의 오버플로는 정의된 wrap-around지만, 부호 있는 오버플로는 표준상 동작이 보장되지 않는다.)

### 2. 곱한 다음에 비교하기

```c
if (width * height * channels > INT_MAX) { ... }   // 🚨
```

이 조건문은 **비교하기도 전에 `width*height*channels`를 계산하면서 이미 오버플로**가 난다. "따르기 전에 확인"해야 하는데 "이미 넘치고 나서 확인"하는 셈이라 소용이 없다.


<br/><br/>


## 해결

핵심 원칙은 두 가지다.

1. **곱하기 전에, 나눗셈으로 오버플로를 검사한다.** `a * b > MAX` 대신 `a > MAX / b`로 바꾼다. 나눗셈은 오버플로가 나지 않으므로 안전하게 미리 확인할 수 있다.
2. **한 번에 곱하지 말고 단계별로** 검사한다. `width*height`를 먼저 검사·계산하고, 그 결과에 `channels`를 곱하기 전에 다시 검사한다.

또한 할당 크기와 사용 크기의 **타입을 `size_t`로 통일**해서, `image_new`의 계산과 `image_fill`의 계산이 항상 같은 값이 되도록 했다. 검사 기준도 `int`가 아니라 `size_t`의 최대값인 `SIZE_MAX`를 쓴다.

> `SIZE_MAX`(`<stdint.h>`)는 `size_t`가 표현할 수 있는 최대값이다. `malloc`/`realloc`이 크기를 `size_t`로 받으므로, "할당 가능한 크기의 한계선"이 곧 `SIZE_MAX`다. 값은 플랫폼마다 다르므로(64비트: 약 1.8×10^19, 32비트: 약 42억) 숫자를 직접 박지 않고 `SIZE_MAX`를 쓰면 어느 환경에서도 올바른 한계로 자동으로 맞춰진다.

수정된 구조체와 함수는 다음과 같다.

```c
typedef struct {
    int width;
    int height;
    int channels;
    size_t nbytes;           // ✅ int → size_t (사용 크기 계산과 타입 통일)
    unsigned char *px;
} Image;

static Image *image_new(int width, int height, int channels) {
    Image *img = malloc(sizeof *img);
    if (!img) { perror("malloc"); exit(1); }

    img->width = width;
    img->height = height;
    img->channels = channels;

    // 곱하기 전에 나눗셈으로 검사, 단계별로
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        fprintf(stderr, "overflow: width*height\n");
        exit(1);
    }
    size_t wh = (size_t)width * (size_t)height;   // size_t 곱셈

    if (wh > SIZE_MAX / (size_t)channels) {
        fprintf(stderr, "overflow: width*height*channels\n");
        exit(1);
    }
    img->nbytes = wh * (size_t)channels;          // size_t 곱셈

    img->px = malloc(img->nbytes);
    if (!img->px) { perror("malloc px"); exit(1); }
    return img;
}
```

> 주의: `size_t wh = width * height;`처럼 쓰면 안 된다. 결과를 `size_t`에 담더라도 `width`와 `height`가 `int`면 **곱셈 자체는 int로 수행되어 이미 오버플로**한 값을 담게 된다. 캐스팅은 반드시 곱하기 **전에** 붙여 `(size_t)width * (size_t)height`로 해야 한다. 담는 타입이 아니라 연산에 참여하는 값의 타입이 계산 방식을 결정한다.

<br/><br/>

## 검증 과정에서 만난 SIGKILL

수정 후 `65536×65536×4`로 실행하니 이번엔 곱셈은 제대로 되는데 다른 신호로 종료됐다.

```
max:18446744073709551615
allocated nbytes=17179869184 for 65536x65536 x4

Program terminated with signal SIGKILL, Killed.
```

`nbytes`가 정확히 171억으로 나온다 — 오버플로는 완전히 고쳐졌다는 증거다. `SIGKILL`은 코드 버그가 아니라, 계산된 약 16GB를 `image_fill`이 실제로 모두 채우려다 물리 메모리가 부족해져 리눅스의 **OOM Killer**가 프로세스를 강제 종료한 것이다.

여기서 중요한 점은, 이것이 오히려 **안전해졌다는 신호**라는 것이다. 예전 버그 코드는 0바이트짜리 버퍼에 16GB를 써서 조용히 힙을 손상시켰다(UB). 수정 후에는 정직하게 16GB를 요청하므로, 감당이 안 되면 OS가 명확하게 차단한다.

작은 크기로 바꾸면 끝까지 정상 실행된다.

```
Image *img = image_new(655, 655, 4);

allocated nbytes=1716100 for 655x655 x4
px[0]=255
[Inferior 1 (process 50942) exited normally]
```
<br/>

### 왜 몇 군데만 접근하면 큰 크기도 실행되나 — 가상 메모리와 디맨드 페이징

`malloc`은 물리 메모리를 바로 주지 않는다. **가상 주소 공간을 쓸 수 있게 해주는 약속**만 하고, 실제 물리 메모리는 그 주소에 진짜로 접근하는 순간 **페이지 단위**(리눅스 기본 4KB)로 연결된다(디맨드 페이징). 미연결 페이지에 접근하면 **페이지 폴트**가 발생하고, 그때 OS가 해당 페이지 하나만 물리 RAM에 연결한다.

```c
img->px[0] = 0xFF;                // 첫 페이지 접근 → 4KB만 물리 할당
img->px[img->nbytes - 1] = 0xFF;  // 마지막 페이지 접근 → 4KB 더 할당
// 총 물리 메모리 사용: 약 8KB (16GB 예약 중에)
```

`image_fill`처럼 버퍼 전체를 채우면 모든 페이지가 물리 메모리를 요구해 16GB가 실제로 필요해지지만, 양 끝 두 군데만 건드리면 그 두 페이지(약 8KB)만 실제로 쓰인다. 그래서 큰 할당도 "전체를 채우지 않으면" 실행될 수 있다.

## 교훈

- 크기를 곱셈으로 계산해 할당할 때는 **곱하기 전에** 오버플로를 검사한다. `a * b > MAX`가 아니라 `a > MAX / b`.
- 검사조차 단계별로 나눠서, 중간 곱셈에서 오버플로가 나지 않게 한다.
- **담는 타입이 아니라 연산에 참여하는 값의 타입**이 계산을 결정한다. `size_t`로 곱하려면 피연산자를 곱하기 전에 캐스팅한다.
- 타입을 키우는 것(`int` → `long long`)은 해결이 아니다. 한계만 밀릴 뿐이고, 부호 있는 오버플로는 UB다.
- **할당 크기 계산과 사용 크기 계산의 타입을 일치**시킨다. 둘이 어긋나면 검사를 잘해도 다른 경로에서 힙 오버플로가 난다.
- 크기·개수·인덱스에는 `int` 대신 `size_t`를, 검사 기준으로는 `SIZE_MAX`를 쓴다.

<br/><br/>

## 남은 개선점 (선택)

핵심 취약점은 해결됐지만, 견고함을 위해 추가하면 좋은 것들:

- **음수/0 방어**: `width`, `height`, `channels`가 0이면 `SIZE_MAX / height`에서 0으로 나누기가 발생하고, 음수는 `(size_t)`로 캐스팅될 때 거대한 값으로 뒤집힌다. 캐스팅·나눗셈 전에 `if (width <= 0 || height <= 0 || channels <= 0)`로 걸러낸다.


