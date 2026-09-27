
# 08_uninitialized_read
<br/>

## 개념

> **Uninitialized read**
> 초기화하지 않은 변수(메모리)의 값을 읽어서 사용하는 메모리 버그.
> 흔히 "쓰레기 값(garbage value)을 읽는다"고 표현한다.

<br/><br/>

## 1. 크래시 재현

```
Program received signal SIGSEGV, Segmentation fault.
0x0000aaaaaaaa0a24 in row_sum (rows=0xaaaaaaac12a0, nrows=32)
    at challenges/08_uninitialized_read/bug.c:34
34                  total += rows[i][j];
```

```
(gdb) bt
#0  0x0000aaaaaaaa0a24 in row_sum (rows=0xaaaaaaac12a0, nrows=32)
    at challenges/08_uninitialized_read/bug.c:34
#1  0x0000aaaaaaaa0aac in main () at challenges/08_uninitialized_read/bug.c:46
```

`row_sum`이 `rows[i][j]`를 역참조하는 시점에 세그폴트가 발생한다.

<br/><br/>

## 2. 코드 구조 파악

### `dirty_heap()`

`malloc`으로 힙 메모리를 할당받아 `0xAB`로 전부 채운 뒤, 별도 초기화 없이 바로 `free`한다.

```c
static void dirty_heap(void) {
    void *scratch = malloc(ROWS * sizeof(int *));
    if (scratch) {
        memset(scratch, 0xAB, ROWS * sizeof(int *));
        free(scratch);
    }
}
```

`free`된 메모리의 **내용물은 지워지지 않고** `0xAB` 패턴 그대로 남는다. 이 메모리를 나중에 다른 곳에서 재할당받아 쓰면서 제대로 초기화하지 않으면, 그 쓰레기 값이 그대로 드러난다.

### `make_matrix()`의 문제

```c
for (int i = 0; i < ROWS; i += 2) {   // ⚠️ 짝수 인덱스만 채움
    int *r = malloc(COLS * sizeof(int));
    for (int j = 0; j < COLS; j++) r[j] = i * COLS + j;
    rows[i] = r;
}
```

`rows` 배열은 `ROWS`(32)개 슬롯 전체를 `malloc`했지만, **짝수 인덱스만 채우고 홀수 인덱스는 그대로 방치**된다.

### `row_sum()`의 문제

```c
for (int i = 0; i < nrows; i++) {   // 홀수 인덱스까지 전부 읽음
    for (int j = 0; j < COLS; j++) {
        total += rows[i][j];   // 🚨 홀수 i에서 크래시
    }
}
```

`make_matrix`는 짝수만 채웠는데, `row_sum`은 **모든 인덱스(0~31)를 순회**한다. → 불일치.

<br/><br/>

## 3. GDB로 실제 메모리 확인

### `rows` 배열 내부 확인

```
(gdb) info locals
rows = 0xaaaaaaac12a0

(gdb) x/104bx rows
0xaaaaaaac12a0: 0xb0 0x13 0xac 0xaa 0xaa 0xaa 0x00 0x00   ← 정상 포인터 (짝수 슬롯)
0xaaaaaaac12a8: 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
0xaaaaaaac12b0: 0xd0 0x13 0xac 0xaa 0xaa 0xaa 0x00 0x00   ← 정상 포인터
0xaaaaaaac12b8: 0xab 0xab 0xab 0xab 0xab 0xab 0xab 0xab   ← 쓰레기 값 (홀수 슬롯)
0xaaaaaaac12c0: 0xf0 0x13 0xac 0xaa 0xaa 0xaa 0x00 0x00
0xaaaaaaac12c8: 0xab 0xab 0xab 0xab 0xab 0xab 0xab 0xab
```

중간중간 `0xab`로 채워진 슬롯이 보인다 — `dirty_heap`이 남긴 흔적이 그대로 남아있는 것.

### `scratch` 해제 직전 상태 확인

```
(gdb) print scratch
$3 = (void *) 0xaaaaaaac12a0

(gdb) x/104bx 0xaaaaaaac12a0
0xaaaaaaac12a0: 0xab 0xab 0xab 0xab 0xab 0xab 0xab 0xab
0xaaaaaaac12a8: 0xab 0xab 0xab 0xab 0xab 0xab 0xab 0xab
...
```

`free` 직전엔 전체가 `0xab`로 채워져 있음을 확인.

### `rows`가 `scratch`와 같은 주소를 재사용하는지 확인

```
(gdb) print rows
$4 = (int **) 0xaaaaaaac12a0
```

`scratch`가 있던 바로 그 주소를 `rows`가 그대로 물려받는다.

<br/><br/>

## 4. 왜 같은 주소가 재사용되는가 — malloc 할당자 동작 원리

`scratch`와 `rows`는 **같은 크기**(`ROWS * sizeof(int *)`)로 할당되기 때문에, malloc 할당자가 방금 해제한 블록을 그대로 재사용할 가능성이 높다.

```
1. free(scratch) 호출
   → 이 블록이 "비어있음" 상태로 free list(가용 목록)에 등록됨
   → 메모리는 OS에 반납되지 않고 프로세스가 계속 보유

2. malloc(같은 크기) 호출 (= rows 할당)
   → 할당자가 free list에서 딱 맞는 크기의 블록을 발견
   → 새로 얻어오지 않고 그 블록을 그대로 재사용
   → scratch가 쓰던 주소를 rows가 그대로 돌려받음
```

OS에 메모리를 매번 새로 요청/반납하는 비용을 줄이기 위해, 할당자가 해제된 블록을 캐싱해뒀다가 재사용하는 것 — 이것이 malloc의 일반적인 최적화 동작이다. `malloc`은 이 과정에서 메모리 **내용을 지워주지 않으므로**, 재사용된 블록에는 이전 데이터(`0xab`)가 그대로 남아있다.

<br/><br/>

## 5. 크래시 원인 정리

| 함수 | 순회 방식 | 결과 |
|---|---|---|
| `make_matrix` | `i += 2` (짝수만 채움) | 홀수 인덱스는 쓰레기 포인터(`0xab...`)로 방치 |
| `row_sum` | `i++` (전체 순회) | 홀수 인덱스에서 쓰레기 포인터를 역참조 → **SIGSEGV** |

`i = 0`(짝수)일 때는 정상 진행되다가, `i = 1`(홀수)로 넘어가는 순간 `rows[1]`이 `0xabababababababab`라는 유효하지 않은 주소를 가리키고 있어 역참조 시 크래시가 발생한다.

<br/><br/>

## 6. 수정

`make_matrix`의 순회 방식을 `row_sum`과 일치시킨다.

```diff
- for (int i = 0; i < ROWS; i += 2) {
+ for (int i = 0; i < ROWS; i++) {
      int *r = malloc(COLS * sizeof(int));
      for (int j = 0; j < COLS; j++) r[j] = i * COLS + j;
      rows[i] = r;
  }
```

`rows` 배열의 모든 슬롯(0~31)을 실제로 채워서, `row_sum`이 어떤 인덱스를 읽어도 쓰레기 값이 아닌 유효한 포인터를 참조하도록 한다.

> 함께 확인할 것: `free` 루프도 동일한 순회 범위(`i++`)로 맞춰야 `make_matrix`에서 할당한 모든 행이 누수 없이 해제된다.

```diff
- for (int i = 0; i < ROWS; i += 2) free(rows[i]);
+ for (int i = 0; i < ROWS; i++) free(rows[i]);
  free(rows);
```

<br/><br/>

## 핵심 교훈

- `malloc`은 메모리를 **할당만** 할 뿐 내용을 초기화하지 않는다 (0으로 채우려면 `calloc` 또는 `memset` 필요).
- `free`된 메모리는 내용이 지워지지 않고, 이후 malloc이 그 주소를 그대로 재사용할 수 있다.
- 배열의 일부 슬롯만 채우고 나머지를 방치하면, 그 슬롯에는 이전에 해제된 메모리의 잔여 데이터(쓰레기 값)가 남아 예측 불가능한 크래시로 이어질 수 있다.
- 여러 함수가 같은 배열을 다룰 때는 **순회 범위(초기화 범위, 사용 범위, 해제 범위)를 반드시 일치**시켜야 한다.
