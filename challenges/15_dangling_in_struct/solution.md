# 15_dangling_in_struct

구조체 안의 포인터가 `free` 이후에도 해제된 메모리를 계속 가리키는 **dangling pointer** 문제와, 그로 인한 **use-after-free(UAF)** 크래시를 재현하고 수정한 예제입니다.

<Br/>

## 실행 결과 (수정 전)

```
Starting program: /work/build/15_dangling_in_struct
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".
first request allowed=1
audit:logout

Program received signal SIGBUS, Bus error.
0x006c3a7469647561 in ?? ()
```

<Br/><br/>

## 코드 구조

### `PermFn` 타입

```c
typedef int (*PermFn)(const char *action);
```

`PermFn`은 **"`const char *`를 받아 `int`를 반환하는 함수를 가리키는 포인터"에 붙인 타입 이름**입니다. 함수 이름이나 변수가 아니라 `int`, `char`처럼 선언에 쓰는 자료형입니다.

- `int` : 가리키는 함수의 반환 타입
- `(*PermFn)` : `PermFn`이 포인터 타입임을 나타냅니다. 괄호가 없으면 `int *PermFn(...)`, 즉 "`int *`를 반환하는 함수"라는 전혀 다른 의미가 됩니다.
- `(const char *action)` : 문자열 하나를 인자로 받습니다. `const`는 함수 안에서 문자열 내용을 수정하지 않겠다는 약속입니다.

### 관련 구조체

- `User` : 권한 검사 함수 포인터(`permission`), `uid`, `name`을 가집니다.
- `Session` : 로그인한 `User`를 가리키는 포인터(`user`)를 가집니다.

<Br/><br/>

## 원인 분석

1. `logout()`에서 `free(s->user)`로 `User`를 해제하지만, `s->user`는 여전히 해제된 주소를 가리킵니다 (dangling pointer).
2. 이어서 `audit_record()`가 **`sizeof(User)`와 같은 크기**로 `malloc`하면서, 할당자가 방금 해제된 `User` 자리를 그대로 재사용합니다. 그 자리는 `0xAB`와 문자열 `"audit:logout"`으로 덮어써집니다.
3. 그 뒤 `handle_request()`가 `s->user->permission(action)`을 호출하면, 함수 주소가 있어야 할 자리에 문자열 바이트가 들어 있으므로 엉뚱한 주소로 점프합니다.

크래시 주소가 이를 그대로 보여 줍니다. `0x006c3a7469647561`을 리틀 엔디언으로 읽으면 다음과 같습니다.

| 바이트 | `61` | `75` | `64` | `69` | `74` | `3a` | `6c` | `00` |
|---|---|---|---|---|---|---|---|---|
| 문자 | `a` | `u` | `d` | `i` | `t` | `:` | `l` | |

즉, 함수 포인터 자리에 `"audit:l"`이 들어가 있었습니다. AArch64에서는 명령어 주소가 4바이트 정렬되어야 하는데 이 주소는 정렬되지 않았기 때문에 SIGSEGV가 아닌 **SIGBUS**가 발생했습니다.

<br/><br/>

## 해결 방법

### 1. `free` 후 포인터를 `NULL`로 초기화

해제된 상태임을 포인터 값으로 명확히 표시합니다.

```c
static void logout(Session *s) {
    free(s->user);
    s->user = NULL;
}
```

### 2. 사용 전에 `NULL` 검사

로그아웃된 세션으로 요청이 들어오면 함수 포인터를 호출하지 않고 거부합니다.

```c
static int handle_request(Session *s, const char *action) {
    if (!s || !s->user || !s->user->permission)
        return 0;
    return s->user->permission(action);
}
```

수정 후에는 로그아웃 뒤의 요청이 해제된 메모리에 접근하지 않고 `allowed=0`을 반환합니다.



