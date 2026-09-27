# 11_global_overflow


<br/>

## 개념

### `snprintf`

`printf`처럼 형식화된 문자열을 만들되, 결과를 화면이 아니라 버퍼(문자열)에 저장하고, 버퍼 크기를 넘지 않도록 안전하게 제한하는 함수.

```c
// str    : 결과를 저장할 버퍼
// size   : 그 버퍼의 전체 크기(널 종료 문자 포함)
// format : printf와 동일한 형식 문자열
int snprintf(char *str, size_t size, const char *format, ...);
```

- 절대 버퍼 크기를 넘지 않는다. `size`를 넘어서는 내용은 잘라내고, 반드시 마지막에 `\0`을 넣는다.
- truncation(잘림) 탐지가 가능하다.

<br/><br/>

## 1. 크래시 재현

```
Starting program: /work/build/11_global_overflow
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".

Program received signal SIGSEGV, Segmentation fault.
0x0000fffff7e91984 in ?? () from /lib/aarch64-linux-gnu/libc.so.6
```

<br/><br/>

## 2. 문제 해결 과정

### 2-1. 크래시 시점 확인

메인 함수 `for`문에서 언제 중지되는지 살펴봤다. `i`, `total`을 찍어보니 `i = 349`일 때 멈췄다.

### 2-2. 첫 번째 가설 (틀렸음): `total`이 `ARENA_SIZE`를 넘어서 크래시가 난다

처음에는 `ARENA_SIZE`(=4096)를 `total` 값이 넘으면 크래시가 발생하는 것이라 생각했다. `total`이 `arena` 배열의 전체 사용량을 나타낸다고 착각한 것이다. 그런데 `total`을 정의하는 코드는 다음과 같다.

```c
total += (long)strlen(last);
```

`strlen`은 문자열의 길이, 즉 **널 문자를 제외한** 문자 개수만 센다. 그런데 실제로 `arena` 배열에 문자열을 저장할 때는 **매번 널 문자까지 함께** 저장한다. 따라서 `arena`의 실제 사용량을 확인하려면 `total`이 아니라 `arena_off`를 점검해야 한다.

```c
static char *intern(const char *s) {
    size_t n = strlen(s) + 1;   // 널 문자 포함
    // ...
}

static void *arena_alloc(size_t n) {
    // ...
    void *p = &arena[arena_off];
    arena_off += n;             // 널 문자까지 포함해서 실제 저장 바이트 수를 누적
    return p;
}
```

이런 식으로 널 문자까지 포함(`n` 계산 시 `+1`)해서 실제로 `arena`에 저장된 바이트 수를 센다. 따라서 `arena_off` 값이 `i = 349` 이전, 즉 `i = 348`일 때 이미 4096에 도달한 것이다.

### 2-3. 진짜 원인: `arena_alloc`에 경계 체크가 없음

현재 코드에서는 이렇게 오버플로우가 발생했을 때 대비가 되어있지 않다. 실제로, 오버플로우가 나기 전에는 `arena_off` 값이 정상적으로 커지다가, 4096을 넘치는 순간 이상한 값이 나오게 된다.

<br/><br/>

## 3. 왜 `arena_off` 값이 이상해지는가

`arena`와 `arena_off`가 정의된 코드를 살펴보면:

```c
static unsigned char arena[ARENA_SIZE];   // 4096바이트
static size_t arena_off = 0;               // 그 바로 다음에 이어붙어 선언됨
```

두 변수 모두 전역(static) 변수이기 때문에, 컴파일러는 이 둘을 메모리상에서 연속된 위치에 배치할 가능성이 높다.

```
메모리 레이아웃 (개념도):

주소:        0x1000              0x1000+4096(=0x2000)   0x2000+8
           ┌──────────────────────┬──────────────────────┐
           │   arena[0..4095]     │     arena_off         │
           │     (4096바이트)      │   (size_t, 8바이트)    │
           └──────────────────────┴──────────────────────┘
                                   ↑
                          arena의 "바로 다음" 위치
```

`arena`가 이미 4096바이트를 넘어서면서 오버플로우가 발생했기 때문에, `arena_off` 값도 그 침범을 당해서 이상한 값이 나오게 되는 것이다.

### 오버플로우가 정확히 어떻게 일어나는가

```c
static void *arena_alloc(size_t n) {
    if (arena_off == ARENA_SIZE) {   // 🚨 정확히 "같을 때"만 체크
        return NULL;
    }
    void *p = &arena[arena_off];
    arena_off += n;
    return p;
}
```

`arena_off`가 4096에 거의 도달했지만 아직 정확히 같지는 않은 상태(예: 4090)에서 `n`(예: 20)짜리 요청이 들어오면 체크를 통과한다. 그 뒤 `intern`에서 `memcpy(dst, s, n)`이 실행되면서 `arena[4090] ~ arena[4109]`에 데이터를 쓰게 되는데, `arena[4096]`부터는 이미 `arena` 배열의 범위를 벗어난 곳이며, 바로 그 자리가 `arena_off` 변수 자신이 저장된 메모리다.

`memcpy`가 복사하는 내용은 문자열 데이터(`"insert-347\0"` 같은 것)이므로, 이 바이트들이 `arena_off`가 저장된 위치에 그대로 덮어써지면 `arena_off`의 값이 문자열의 아스키 코드 값들로 재해석되어 완전히 엉뚱한 숫자가 된다. 처음엔 정상적으로 커지다가 "갑자기" 이상해지는 것처럼 보이는 이유는, 오버플로우가 발생하는 순간과 그 오버플로우가 `arena_off` 자신을 침범하는 순간이 거의 동시이기 때문이다.

<br/><br/>


## 4. 수정

경계 체크를 "정확히 같은가"가 아니라 "이번 요청까지 더했을 때 넘치는가"로 바꿔야 한다.

```diff
static void *arena_alloc(size_t n) {
-   if (arena_off == ARENA_SIZE) {
+   if (arena_off + n > ARENA_SIZE) {
        return NULL;
    }
    void *p = &arena[arena_off];
    arena_off += n;
    return p;
}
```

<br/><br/>

## 핵심 교훈

- 전역(static) 변수는 메모리상에서 선언 순서대로 인접하게 배치될 수 있다. 한 버퍼가 오버플로우되면, 바로 다음에 선언된 다른 전역 변수의 메모리를 침범할 수 있다.
- 경계 체크를 "정확히 같을 때(`==`)"로 하면, 매번 다른 크기(`n`)만큼 증가하는 값은 그 경계를 정확히 밟지 못하고 지나쳐버릴 수 있다. 반드시 "이번 요청까지 포함했을 때 넘치는가(`arena_off + n > SIZE`)"로 체크해야 한다.
- 크래시가 발생한 변수(`total`)가 곧 근본 원인이라고 단정하지 말고, 그 변수가 실제로 무엇을 세고 있는지(`\0` 포함 여부 등) 정확히 코드를 따라가며 확인해야 한다.
- 값이 "정상적으로 커지다가 갑자기 이상해진다"면, 그 변수 자체의 계산이 잘못된 것이 아니라 **다른 곳에서 그 변수의 메모리 위치를 침범했을 가능성**을 의심해봐야 한다.