# 13.Linked List Use-After-Free (UAF)

연결 리스트에서 노드를 순회하며 해제할 때 발생한 **use-after-free** 버그를 디버깅하고 수정한 기록.

<br/>

## 1. 문제 상황

프로그램 실행 시 `SIGSEGV`(세그멘테이션 폴트)로 크래시가 발생했다.

```
Program received signal SIGSEGV, Segmentation fault.
0x0000aaaaaaaa0b30 in filter_jobs (head=0xaaaaaaae0680, threshold=5, audit=0xffffffffef30)
    at challenges/13_linked_list_uaf/bug.c:44
44              if (cur->priority < threshold) {
```

크래시가 난 위치는 `filter_jobs` 안에서 `cur`이 가리키는 노드의 필드를 참조하는 지점이었다.

<br/>

## 2. 원인 분석

문제가 된 기존 코드는 다음과 같다.

```c
while (cur != NULL) {
    if (cur->priority < threshold) {
        audit_add(audit, cur->id);
        job_release(cur);   // (1) cur이 가리키는 노드를 free
        cur = cur->next;    // (2) 이미 해제된 메모리를 읽음  ← 문제
    } else {
        nx = cur->next;
        cur->next = NULL;
        if (keep_tail) keep_tail->next = cur; else keep = cur;
        keep_tail = cur;
        cur = nx;
    }
}
```

`job_release(cur)`(내부적으로 `free(cur)`)로 노드를 해제한 **직후에** `cur->next`를 읽는 것이 원인이다.

여기서 흔히 오해하기 쉬운 점은, `free(cur)`가 `cur` **포인터 값 자체**를 바꾸지는 않는다는 것이다. `cur`은 해제 후에도 여전히 같은 주소를 가리킨다. 바뀌는 것은 그 **주소가 가리키는 메모리의 내용**으로, 해제된 뒤에는 allocator가 그 영역을 재사용하거나 오염시킬 수 있다. 따라서 `cur->next`를 읽는 순간 이미 해제된 메모리를 읽게 되고(use-after-free), 거기서 얻은 쓰레기 값을 다음 `cur`로 삼아 `cur->priority`에 접근하면 크래시가 난다.

<br/>

### gdb로 확인

`job_release` **실행 전**에는 `cur`을 통해 노드에 정상적으로 접근된다.

```
(gdb) print cur
$35 = (Job *) 0xaaaaaaae0660
(gdb) print cur->id
$36 = 3999
```

`job_release`(= `free`) **실행 후**에는 `cur` 포인터 값은 그대로지만, 그 메모리가 담고 있던 값이 오염된 것을 볼 수 있다.

```
(gdb) print cur
$37 = (Job *) 0xaaaaaaae0660     ← 주소는 동일
(gdb) print cur->id
$38 = -1431655712                ← 해제된 메모리의 쓰레기 값
```

`id`가 이렇게 오염되었다는 것은 노드 전체(당연히 `next` 필드 포함)가 더 이상 유효하지 않다는 뜻이다. 이 상태에서 `cur = cur->next`를 실행하면 유효하지 않은 주소가 `cur`에 들어가고, 다음 반복의 `if (cur->priority < threshold)`에서 그 주소를 역참조하다 `SIGSEGV`가 발생한다.

<br/>

## 3. 해결

핵심은 **해제하기 전에 다음 노드 주소를 먼저 저장해 두는 것**이다. 이미 `else` 분기에서 쓰고 있던 `nx` 변수를 동일하게 활용했다.

```c
while (cur != NULL) {
    if (cur->priority < threshold) {
        audit_add(audit, cur->id);
        nx = cur->next;     // (1) 해제 전에 다음 노드를 먼저 저장
        job_release(cur);   // (2) 그다음 해제
        cur = nx;           // (3) 저장해 둔 값으로 이동
    } else {
        nx = cur->next;
        cur->next = NULL;
        if (keep_tail) keep_tail->next = cur; else keep = cur;
        keep_tail = cur;
        cur = nx;
    }
}
```

`audit_add(audit, cur->id)`가 `job_release`보다 앞에 있는 것도 중요하다. `cur->id`를 해제 전에 읽어야 하므로, 이 순서가 뒤바뀌면 또 다른 use-after-free가 된다.

수정 후에는 크래시 없이 정상적으로 실행된다.

<br/>

## 교훈

- 리스트를 순회하며 노드를 해제할 때는 **`next`를 먼저 저장하고, 그다음에 `free`한다.**
- `free`는 포인터 값을 바꾸지 않는다. 바뀌는 것은 그 포인터가 가리키는 메모리의 유효성이다. 그래서 해제 후 dangling pointer는 겉보기에 멀쩡해 보여도 역참조하면 안 된다.
- 해제할 노드에서 값을 꺼내 써야 한다면(`cur->id` 등) 반드시 `free` **이전에** 읽는다.
