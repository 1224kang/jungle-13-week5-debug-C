# realloc_dangling (EditBuffer undo)

## 개념

> **realloc_dangling**
> `realloc`이 메모리를 새 위치로 옮긴 뒤에도, 예전 주소를 그대로 들고 있는 다른 포인터가 남아있어서 발생하는 dangling pointer 버그.

<br/>

### `realloc`에 대해

```c
void *realloc(void *ptr, size_t new_size);
```

- 기존 메모리 뒤에 여유 공간이 있으면 **제자리에서 확장** (반환값 == `ptr`)
- 여유 공간이 없으면 **새 블록을 할당하고 데이터를 복사한 뒤, 기존 `ptr`이 가리키던 메모리는 내부적으로 `free`함** (반환값 != `ptr`)

즉 `realloc` 호출 후에는, 그 메모리를 가리키던 **다른 모든 포인터**가 예전 주소를 그대로 들고 있다면 전부 dangling pointer가 될 수 있다.

<br/><br/>

## 문제 코드

```c
static void eb_snapshot(EditBuffer *e) {
    if (e->undo_n < MAX_UNDO) e->undo[e->undo_n++] = e->data;   // 🚨
}
```

`eb_snapshot`이 `e->data`의 **내용을 복사하는 게 아니라, 주소값 자체를 그대로** `undo[]`에 저장한다.

```
스냅샷 시점:
  e->data  ──────┐
                 ▼
              0xAAA  (실제 데이터)
                 ▲
  e->undo[0] ────┘   ← 같은 주소를 가리킴 (복사본 아님, 별칭일 뿐)
```

이후 `eb_push`가 반복 호출되며 `eb_grow` 내부의 `realloc`이 메모리를 새 위치로 옮기면:

```c
static void eb_grow(EditBuffer *e, size_t need) {
    size_t nc = e->cap;
    while (nc < need) nc *= 2;
    int *p = realloc(e->data, nc * sizeof(int));
    if (!p) { perror("realloc"); free(e->data); exit(1); }
    e->data = p;      // e->data는 새 주소로 정상 갱신됨
    e->cap = nc;
}
```

```
eb_grow 이후 (realloc이 메모리를 옮겼다고 가정):

  e->data    ──────────────▶ 0xBBB  (새 메모리, realloc이 옮긴 곳)
  e->undo[0] ──────────────▶ 0xAAA  (이미 realloc 내부에서 free된 메모리!)
                              ↑
                         dangling pointer
```

`e->data`는 정상적으로 새 주소를 가리키게 되지만, `e->undo[0]`은 여전히 예전 주소를 들고 있다. 이 상태에서 `undo[0]`을 읽으면 use-after-free, `free(undo[0])`을 호출하면 `e->data`를 해제할 때와 같은 메모리를 또 해제하는 **double free**가 된다.

<br/><br/>

### 처음 접근한 방식 - 임시방편 (근본 해결 아님)

```c
static void eb_free(EditBuffer *e) {
    free(e->data);
    free(e->clipboard);
    for (int i = 0; i < e->undo_n; i++) {
        e->undo[i] = NULL;
        // free(e->undo[i]); // 🚨 double free 위험 때문에 주석 처리해서 회피
    }
    e->undo_n = 0;
    e->data = NULL;
}
```

`free(e->undo[i])`를 주석 처리하면 double free는 피할 수 있지만, **undo 기능 자체가 무의미해진다.** `undo[i]`가 애초에 "그 시점의 데이터"가 아니라 "이미 realloc에 의해 옮겨지거나 재사용된 메모리"를 가리키고 있기 때문에, 되돌리려 해도 의미 있는 데이터를 얻을 수 없다.

<br/><br/>

## 중간 시도 — 여전히 버그가 남아있는 수정

```c
static void eb_snapshot(EditBuffer *e) {
    if (e->undo_n < MAX_UNDO) {
        int *copy = malloc(e->len * sizeof(int));
        if (!copy) { perror("malloc"); exit(1); }
        memcpy(copy, e->data, e->len * sizeof(int));   // copy에는 제대로 복사함
        e->undo[e->undo_n++] = e->data;                // 🚨 그런데 저장은 여전히 e->data!
    }
}
```

`copy`를 새로 `malloc`하고 `memcpy`까지 정확히 했지만, 정작 `undo[]`에 넣는 값은 여전히 `e->data`다. 그 결과:

- 애써 만든 `copy`는 아무도 참조하지 않아 그대로 **메모리 누수**가 되고
- `undo[0]`은 여전히 `e->data`와 **같은 주소**를 가리켜서 dangling 문제가 그대로 남는다

<br/><br/>

## 최종 해결

`undo[]`에 저장하는 값을 `e->data`가 아니라 새로 만든 `copy`로 바꾼다. 딱 한 단어만 다르다.

```diff
static void eb_snapshot(EditBuffer *e) {
    if (e->undo_n < MAX_UNDO) {
        int *copy = malloc(e->len * sizeof(int));
        if (!copy) { perror("malloc"); exit(1); }
        memcpy(copy, e->data, e->len * sizeof(int));
-       e->undo[e->undo_n++] = e->data;
+       e->undo[e->undo_n++] = copy;
    }
}
```

```
스냅샷 시점:
  e->data  ──▶ 0xAAA  (원본, 이후 realloc으로 계속 옮겨질 수 있음)
  undo[0]  ──▶ 0xCCC  (독립된 복사본, e->data와 완전히 무관)
```

이제 `e->data`가 `eb_grow`의 `realloc`을 거쳐 몇 번을 옮겨 다니든, `undo[0]`은 스냅샷 시점에 독립적으로 할당받은 자기만의 메모리(`0xCCC`)를 그대로 가리키므로 전혀 영향을 받지 않는다.

### `eb_free`도 이제 정상적으로 `free`하면 된다

```c
static void eb_free(EditBuffer *e) {
    free(e->data);
    e->data = NULL;

    free(e->clipboard);
    e->clipboard = NULL;

    for (int i = 0; i < e->undo_n; i++) {
        free(e->undo[i]);   // copy는 e->data와 별개의 메모리이므로 double free 아님
        e->undo[i] = NULL;
    }
    e->undo_n = 0;
}
```

`undo[i]`가 이제 진짜 소유권을 가진 독립 메모리이므로, 정리할 때 반드시 `free`해줘야 한다 (안 하면 이번엔 메모리 누수가 된다).

<br/><br/>

## 정리

| 단계 | `undo[]`에 저장하는 값 | 문제 |
|---|---|---|
| 최초 코드 | `e->data` (원본 주소 그대로) | realloc 후 dangling, free 시 double free 위험 → `free(undo[i])` 주석 처리로 임시 회피 |
| 중간 시도 | `copy`를 만들었지만 저장은 여전히 `e->data` | `copy`는 메모리 누수, `undo[]`는 여전히 dangling |
| 최종 수정 | `copy` (복사본) | realloc과 완전히 독립적, `free(undo[i])`도 안전하게 가능 |

<br/><br/>

## 핵심 교훈

- `realloc`은 메모리를 옮길 수 있고, 옮기면 예전 주소를 참조하던 **다른 모든 포인터가 무효화**된다.
- "스냅샷"이나 "백업"을 만들 때는 **포인터만 복사(별칭)**하면 안 되고, **데이터 자체를 복사(malloc + memcpy)**해서 원본과 독립적인 소유권을 가져야 한다.
- 복사본을 만들었다면, 정리(`free`) 코드도 그 복사본을 실제로 해제하도록 갱신해야 한다 — 안 그러면 메모리 누수로 이어진다.
- "에러 없이 실행된다"는 것이 "버그가 없다"는 뜻은 아니다. 지금 코드처럼 실제로 dangling 포인터를 밟는 코드 경로가 없으면 크래시는 안 나지만, 그 포인터를 사용하는 코드 경로가 추가되는 순간 언제든 터질 수 있는 잠재적 버그다.