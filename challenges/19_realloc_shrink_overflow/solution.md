# 19_realloc_shrink_overflow

`realloc`으로 버퍼를 **줄인 뒤 길이(`len`)를 갱신하지 않아서**, 줄어든 버퍼 너머를 읽는 **힙 버퍼 오버리드(heap buffer over-read)** 문제를 분석하고 수정한 예제이다.

<br/><br/>

## 실행 결과 (수정 전)

```
Starting program: /work/build/19_realloc_shrink_overflow
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".

Program received signal SIGSEGV, Segmentation fault.
0x0000aaaaaaaa0b3c in signal_energy (s=0xffffffffef10) at challenges/19_realloc_shrink_overflow/bug.c:27
27              e += s->samples[i] * s->samples[i];
```

`signal_energy`의 반복문에서 `s->samples[i]`를 읽다가 SIGSEGV가 발생한다.


<br/><br/>

## 코드 구조

```c
typedef struct {
    double *samples;   /* double 배열을 가리키는 포인터 */
    size_t  len;       /* 실제로 유효한 원소 개수 */
    size_t  cap;       /* 할당된 공간의 원소 개수 */
} Signal;
```

1. `signal_init(&s, 2000000)` : `double` 2,000,000개 크기의 배열을 할당하고 `len = cap = 2000000`으로 설정한다.
2. `signal_trim(&s, 8)` : `realloc`으로 배열을 8칸으로 줄인다.
3. `signal_energy(&s)` : `len`만큼 반복하며 각 샘플의 제곱을 더한다.


<br/><br/>

## 원인 분석

수정 전 `signal_trim`은 다음과 같다.

```c
static void signal_trim(Signal *s, size_t keep) {
    if (keep > s->cap) return;
    double *p = realloc(s->samples, keep * sizeof(double));
    if (p) s->samples = p;
    s->cap = keep;
    /* len은 갱신하지 않음 */
}
```


<br/>

### 처음에 오해했던 부분

처음에는 `realloc`의 크기 인자가 기존 배열 뒤에 **추가로** 붙일 크기라고 생각했다. 하지만 실제로는 블록의 **새 전체 크기**를 지정하는 것이었다. 즉 `realloc(s->samples, 8 * sizeof(double))`은 "8칸 더"가 아니라 "전체를 8칸으로"라는 뜻이고, 200만 칸짜리 배열을 8칸짜리로 **줄인다.**

이때 블록은 제자리에서 줄어들 수도, 다른 곳으로 옮겨질 수도 있지만, 어느 경우든 앞쪽 8칸의 내용은 유지된다.

<br/>

### 실제 문제

버퍼는 8칸으로 줄었는데 `s->len`은 여전히 **2,000,000**으로 남아 있다. `signal_energy`는 `len`을 믿고 반복하므로, 할당된 8칸을 넘어 그 뒤의 메모리를 계속 읽게 된다. 읽기를 이어 가다가 프로세스에 매핑되지 않은 주소에 도달하는 순간 SIGSEGV가 발생한다.

즉 문제는 블록이 옮겨졌는지 여부가 아니라, **실제 버퍼 크기와 `len`이 서로 맞지 않게 된 것**이다.


<br/><br/>

## 해결

trim할 때 `cap`뿐 아니라 `len`도 **원소 개수 기준으로** 함께 갱신한다.

```c
static void signal_trim(Signal *s, size_t keep) {
    if (keep > s->cap) return;
    double *p = realloc(s->samples, keep * sizeof(double));
    if (p) s->samples = p;   /* 성공했을 때만 포인터 교체 */
    s->cap = keep;
    s->len = keep;           /* 추가: 줄어든 크기에 맞춰 길이 갱신 */
}
```

`len`은 바이트 수가 아니라 **원소 개수**이므로 `keep * sizeof(double)`(64)이 아니라 `keep`(8)을 넣어야 한다.


<br/><br/>

## `realloc` 정리

`realloc`은 이미 `malloc`/`calloc`/`realloc`으로 할당받은 메모리 블록의 크기를 바꾸는 함수이다.

```c
/* ptr : 크기를 바꿀 기존 블록 (힙에서 할당받은 포인터 또는 NULL)
 * size: 새로 원하는 전체 크기 (바이트, 증가량이 아님)
 * 반환값: 새 블록의 주소. 실패하면 NULL */
void *realloc(void *ptr, size_t size);
```

**동작 방식**

- 할당자가 상황에 따라 **제자리에서 크기를 조정**하거나, **새 위치에 할당 → 내용 복사 → 기존 블록 해제**를 한다. 어느 쪽이 될지는 호출하는 쪽에서 알 수 없으므로, 항상 **반환된 주소를 새로 사용**해야 한다.
- 기존 크기와 새 크기 중 **작은 쪽까지의 내용**은 보존된다.
- 늘어난 부분은 **초기화되지 않는다.**

**실패 시 주의**

`realloc`이 실패해 `NULL`을 반환해도 **원래 블록은 그대로 유효**하다. 그래서 결과를 원래 포인터에 바로 대입하면 안 된다.

```c
s->samples = realloc(s->samples, n);   /* 실패 시 원래 주소를 잃어 누수 */

double *p = realloc(s->samples, n);    /* 올바른 패턴 */
if (p) s->samples = p;
```

**기타**

- `realloc(NULL, n)`은 `malloc(n)`과 같다.
- `realloc(ptr, 0)`은 구현마다 동작이 다르고(glibc는 해제 후 `NULL` 반환 가능), C23부터는 정의되지 않은 동작이므로 피해야 한다.
- 블록이 옮겨지면, 그 블록 안을 가리키던 **다른 포인터들은 모두 dangling pointer**가 된다.
