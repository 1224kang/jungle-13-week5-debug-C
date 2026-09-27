# 09_strcpy_overflow

## 개념

> **strcpy_overflow**
> `strcpy`를 사용할 때 대상 버퍼(`dest`)의 크기를 넘어서는 데이터를 복사해서 발생하는 버퍼 오버플로우.

<br/>

### `strcpy`에 대해

문자열을 한 버퍼에서 다른 버퍼로 복사하는 함수.

```c
// dest : 복사해 넣을 대상 버퍼의 시작 주소
// src  : 복사할 원본 문자열
char *strcpy(char *dest, const char *src);
```

`src`의 첫 글자부터 시작해서 `\0`을 만날 때까지 한 글자씩 `dest`에 복사한다. `\0`도 함께 복사되어 `dest`가 올바르게 종료된 문자열이 되도록 한다.


<br/><br/>

## 1. 크래시 재현

```
Program received signal SIGSEGV, Segmentation fault.
0x0000fffff7e8d328 in strcpy () from /lib/aarch64-linux-gnu/libc.so.6
```

```
(gdb) bt
#0  0x0000fffff7e8d328 in strcpy () from /lib/aarch64-linux-gnu/libc.so.6
#1  0x0000aaaaaaaa0b90 in join (parts=0xffffffffef08, n=4)
    at challenges/09_strcpy_overflow/bug.c:20
#2  0x0000aaaaaaaa0c64 in main () at challenges/09_strcpy_overflow/bug.c:36

(gdb) frame 1
#1  0x0000aaaaaaaa0b90 in join (parts=0xffffffffef08, n=4)
    at challenges/09_strcpy_overflow/bug.c:20
20              strcpy(out + off, parts[i]);

(gdb) frame 2
#2  0x0000aaaaaaaa0c64 in main () at challenges/09_strcpy_overflow/bug.c:36
36          char *msg = join(parts, n);
```

`join` 함수 내부의 `strcpy` 호출에서 세그폴트가 발생한다.

<br/><br/>

## 2. 기존 코드 파악

```c
const char *parts[] = { "GET ", "/index.html", "HTTP/1.1\r\n\r\n", body };
```

`parts`는 문자열 자체가 아니라 **문자열들을 가리키는 포인터들의 배열**이다.

```
parts (배열, 32바이트):
┌──────────┬──────────────┬───────────────────┬──────────┐
│ parts[0] │   parts[1]   │      parts[2]      │ parts[3] │
│ (8바이트) │   (8바이트)   │     (8바이트)       │ (8바이트) │
└────┬─────┴──────┬───────┴─────────┬──────────┴────┬─────┘
     │             │                │               │
     ▼             ▼                ▼               ▼
  "GET "    "/index.html"   " HTTP/1.1\r\n\r\n"     body
 (5바이트)     (12바이트)         (14바이트)      (body 길이는 별도, 매우 큼)
```

### `strlen`에 대해

문자열의 길이(문자 개수, `\0` 제외)를 구한다.

```c
size_t strlen(const char *s);
```

`sizeof`와 헷갈리기 쉬운 지점: 이 코드에서 `sizeof(parts[0])`는 8이다 (`parts[0]`이 포인터 변수이므로). 반면 `strlen(parts[0])`은 4다 — 가리키는 문자열 `"GET "`의 실제 길이를 의미한다.

<br/><br/>

## 3. 처음 세운 가설 (틀렸음)

메모리 할당은 `strlen` 기준(널 문자 미포함 길이)으로 받고, `strcpy`는 널 문자까지 포함해서 복사하기 때문에 그 1바이트 차이로 오버플로우가 발생하는 줄 알았다.

**→ 실제로는 그렇지 않았다.** `strcpy`가 널 문자까지 복사하는 부분은 `joined_size`에서 `total = 1`로 애초에 한 바이트를 더 잡아두고 있어서 문제가 없었다.

<br/><br/>

## 4. 진짜 원인

```c
static size_t joined_size(const char *const *parts, int n) {
    size_t total = 1;
    for (int i = 0; i < n - 1; i++) {   // ⚠️ n-1까지만 (body 제외)
        total += strlen(parts[i]);
    }
    return total;
}
```

```c
static char *join(const char *const *parts, int n) {
    size_t need = joined_size(parts, n);
    char *out = malloc(need);
    // ...
    for (int i = 0; i < n - 1; i++) {   // 여기도 n-1까지만
        strcpy(out + off, parts[i]);
        off += strlen(parts[i]);
    }
    // ...
}
```

메모리 할당(`joined_size`)을 할 때는 마지막 `body` 부분을 크기 측정에서 **제외**했다. 그런데 이건 `join`의 복사 루프도 똑같이 `n-1`까지만 돌아서, 당장은 `malloc` 크기와 `strcpy`하는 양이 일치해 문제가 없어 보였다.

문제는 **이후 단계에서 body 부분까지 복사해서 저장하려고 시도**하면서 발생했다. `body`는 20만 바이트에 달하는 큰 문자열인데, 이걸 결과에 포함시키려는 순간 `joined_size`가 계산해준 크기(`body` 제외)와 실제로 복사하려는 양(`body` 포함)이 어긋나면서 할당된 버퍼보다 훨씬 큰 데이터를 `strcpy`하게 되어 **버퍼 오버플로우**가 발생했다.

즉 핵심 원인은 "널 문자 처리"가 아니라, **크기를 계산하는 루프와 실제로 복사하는 루프가 다루는 범위(`n-1` vs `n`)가 일치하지 않았다는 것** — 생각보다 훨씬 단순한 문제였다.

<br/><br/>

## 5. 수정

`body`까지 결과에 포함해야 하므로, 크기 측정과 복사 루프 모두 `n-1`이 아니라 `n`까지 순회하도록 통일한다.

```diff
static size_t joined_size(const char *const *parts, int n) {
    size_t total = 1;
-   for (int i = 0; i < n - 1; i++) {
+   for (int i = 0; i < n; i++) {
        total += strlen(parts[i]);
    }
    return total;
}
```

```diff
static char *join(const char *const *parts, int n) {
    size_t need = joined_size(parts, n);
    char *out = malloc(need);
    if (!out) { perror("malloc"); exit(1); }

    size_t off = 0;
-   for (int i = 0; i < n - 1; i++) {
+   for (int i = 0; i < n; i++) {
        strcpy(out + off, parts[i]);
        off += strlen(parts[i]);
    }
    out[off] = '\0';
    return out;
}
```

기존에는 `n-1`까지만 크기를 측정하고 복사했다면, 이제는 `n`까지, 즉 `body` 부분까지 포함해서 크기 측정과 복사를 모두 수행하도록 했다.

<br/><br/>

## 핵심 교훈

- `strcpy`는 대상 버퍼의 크기를 전혀 모른 채 무조건 복사하므로, **호출 전에 크기를 정확히 계산해두는 쪽의 책임**이 절대적으로 크다.
- 크기를 계산하는 루프(`joined_size`)와 실제로 데이터를 쓰는 루프(`join`)가 **서로 다른 범위**를 순회하면, 당장은 우연히 맞아떨어져도 나중에 다루는 데이터가 바뀌는 순간 오버플로우로 이어질 수 있다.
- 겉으로 보이는 원인(널 문자 처리 방식)과 실제 원인(순회 범위 불일치)이 다를 수 있으므로, 가설을 세운 뒤에도 실제 gdb 백트레이스와 코드의 루프 범위를 직접 대조해서 확인하는 과정이 중요하다.
