# 17_ownership_uaf

같은 메시지를 **구독자와 브로커가 모두 해제**하면서 발생하는 use-after-free / double free 문제를, **소유권(ownership)** 관점에서 분석하고 수정한 예제

## 실행 결과 (수정 전)

```
Starting program: /work/build/17_ownership_uaf
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".
recv #1: hello
recv #2: world
recv #3: broker

Program received signal SIGSEGV, Segmentation fault.
0x0000fffff7e86dcc in free () from /lib/aarch64-linux-gnu/libc.so.6
```

```
(gdb) bt
#0  0x0000fffff7e86dcc in free () from /lib/aarch64-linux-gnu/libc.so.6
#1  0x0000aaaaaaaa0afc in msg_free (m=0xaaaaaaac12a0) at challenges/17_ownership_uaf/bug.c:29
#2  0x0000aaaaaaaa0ca0 in broker_shutdown (b=0xffffffffee48) at challenges/17_ownership_uaf/bug.c:55
#3  0x0000aaaaaaaa0d6c in main () at challenges/17_ownership_uaf/bug.c:69
```

메시지 세 개는 정상적으로 출력된 뒤, `broker_shutdown` → `msg_free` → `free` 에서 크래시가 발생한다.

<Br/><br/>

## 코드 파악

```c
static void on_message(Msg *m) {
    printf("recv #%d: %s\n", m->id, m->body);
    msg_free(m);
}

int main(void) {
    ...
    deliver(&b, on_message);
    ...
}
```

### ❓ 함수 이름만 넘기고 인자를 안 주면 문제가 될까?

 괄호 없이 `on_message`라고 쓰면 함수를 **호출하는 것이 아니라 함수의 주소를 넘기는 것**이다. 인자는 나중에 `deliver` 안에서 그 함수 포인터를 호출할 때(`sub(m)`) 넣어 준다. 이렇게 나중에 호출되도록 넘기는 함수를 **콜백(callback)** 이라고 한다.


<Br/><br/>

## 원인 분석

`publish`는 메시지 하나를 만들어 **두 개의 포인터 배열에 동시에** 넣는다.

```
            ┌──────────────────────┐
inbox[i] ──►│ Msg { id, body } ────┼──► "hello"
log[i]   ──►│                      │
            └──────────────────────┘
```

그리고 이 같은 `Msg`를 두 곳에서 해제한다.

1. `deliver`가 `inbox`에서 메시지를 꺼내 구독자 `on_message`에 전달하고, **구독자가 `msg_free`로 해제**
2. 프로그램 종료 시 `broker_shutdown`이 `log`를 돌면서 **같은 메시지를 다시 `msg_free`** 

즉 이미 해제된 메시지를 한 번 더 해제하는 **이중 해제(double free)** 구조이다.

그런데 크래시는 glibc의 `double free detected` 메시지가 아니라 **SIGSEGV**로 나타났다. 두 번째 `msg_free`가 실행될 때 순서를 보면 이유를 알 수 있다.

```c
static void msg_free(Msg *m) {
    free(m->body);   /* 29번째 줄: 크래시 위치 */
    free(m);
}
```

`free(m->body)`를 하려면 먼저 **이미 해제된 `m`에서 `body` 필드를 읽어야** 한다. 이 읽기 자체가 use-after-free이고, 해제된 메모리는 할당자가 자기 관리 정보를 기록하는 데 재사용하기 때문에 `body` 자리에는 더 이상 올바른 주소가 들어 있지 않다. 그 쓰레기 값을 `free`에 넘기면서 잘못된 메모리에 접근해 SIGSEGV가 발생한 것이다.

## 해결 방향: 누가 소유자인가?

해결책 자체는 간단합니다. **두 곳 중 한 곳에서만 해제**하면 됩니다. 중요한 건 **어느 쪽이 해제 책임(소유권)을 갖는 게 맞는가**이다.

- `Msg`를 **만든 것**은 브로커(`publish`)
- 전체 메시지 목록(`log`)을 **끝까지 들고 있는 것**도 브로커
- 구독자 `on_message`는 받은 메시지를 `printf`로 **보여 주는 역할**만 

따라서 메시지의 소유자는 **브로커**이고, 구독자는 메시지를 **빌려서 읽기만** 해야 한다. 구독자에서 해제하면 브로커가 아직 들고 있는 `log`의 포인터들이 전부 dangling pointer가 된다.

## 배경 지식: 발행-구독(pub/sub) 패턴

이 과제의 `Broker`는 **발행-구독 패턴**을 단순화한 구조이다. 메시지를 보내는 쪽과 받는 쪽이 서로를 직접 알지 못한 채, **중개자(브로커)** 를 통해서만 소통하도록 만드는 설계 패턴이다.

```
Publisher A ─┐                      ┌─► Subscriber X
             ├─► [   Broker   ] ────┼─► Subscriber Y
Publisher B ─┘   (토픽별 분류)       └─► Subscriber Z
```

| 구성 요소 | 역할 | 코드 |
|---|---|---|
| Publisher | 메시지를 만들어 브로커에 보냄 | `publish()` 호출하는 `main` |
| Broker | 메시지를 받아 분류·보관하고, 관심 있는 구독자에게 전달 | `Broker`, `deliver()`, `broker_shutdown()` |
| Subscriber | 원하는 메시지를 브로커에 등록해 두고, 오면 받아서 처리 | `on_message` |

<br/>

**장점: 결합도 분리**

1. **공간적 분리**: 발행자는 구독자가 누구인지 모른다. 구독자를 추가하거나 빼도 발행자 코드는 바뀌지 않는다.
2. **시간적 분리**: 브로커가 메시지를 보관해 두면, 구독자가 그 순간 꺼져 있어도 나중에 받을 수 있다.
3. **동기화 분리**: 발행자는 메시지를 넘기고 바로 자기 일을 계속한다. 구독자가 처리를 끝낼 때까지 기다리지 않는다.


<Br/><br/>

## 수정

구독자 `on_message`에서 `msg_free` 호출을 제거하고, 해제는 브로커의 `broker_shutdown`에서만 하도록 했다.

```c
static void on_message(Msg *m) {
    printf("recv #%d: %s\n", m->id, m->body);
    /* 구독자는 메시지를 빌려 쓰기만 한다. 해제는 브로커 책임 */
}

static void broker_shutdown(Broker *b) {
    for (int i = 0; i < b->log_n; i++) {
        msg_free(b->log[i]);   /* 소유자인 브로커가 한 번만 해제 */
    }
    b->log_n = 0;
}
```

이렇게 하면 각 메시지는 정확히 한 번만 해제되고, `deliver`를 호출하지 않은 경우에도 브로커가 정리하므로 메모리 누수가 생기지 않는다.