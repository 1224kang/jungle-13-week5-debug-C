# 12. free non-heap

> `malloc`, `calloc`, `realloc`, `aligned_alloc`(그리고 이들을 내부에서 쓰는 `strdup` 등)이 **반환한 포인터가 아닌 주소**에 `free()`를 호출하면 **정의되지 않은 동작(Undefined Behavior)** 이 된다.

<br/>

## 1. 문제 상황

CSV 한 줄을 파싱해서 필드별로 출력한 뒤 메모리를 해제하는 프로그램이 `free()`에서 비정상 종료되었다.

```c
static void row_free(Row *r) {
    for (int i = 0; i < r->n; i++) {
        free(r->fields[i]);   // 🚨 문제 지점
    }
    r->n = 0;
}
```

### 디버거 실행 결과

```
Starting program: /work/build/12_free_non_heap
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".
4 fields: [id] [name] [dept] [salary]
free(): invalid pointer

Program received signal SIGABRT, Aborted.
0x0000fffff7e774d8 in ?? () from /lib/aarch64-linux-gnu/libc.so.6
```

파싱과 출력은 정상적으로 끝났고, 해제 단계에서 glibc가 `free(): invalid pointer`를 감지해 `SIGABRT`로 프로그램을 중단시켰다.

<br/><br/>

## 2. 원인 분석

### 2-1. 문자열은 어떻게 함수에 전달될까

```c
static void parse_row(Row *r, const char *csv) { ... }

int main(void) {
    Row r;
    parse_row(&r, "id,name,dept,salary");
    ...
}
```

C에서 문자열은 `'\0'`으로 끝나는 `char` 배열이고, 함수에 넘길 때는 **첫 문자의 주소만** 전달된다. 따라서 `csv`는 `'i'`의 주소를 가리키며, `csv[0] == 'i'`, `csv[3] == 'n'`이고 `'\0'`이 나올 때까지 읽으면 전체 문자열을 순회할 수 있다.

```
csv ──► [i][d][,][n][a][m][e][,][d][e][p][t][,][s][a][l][a][r][y][\0]
```

<br/>

### 2-2. 문자열 리터럴

**문자열 리터럴**은 소스 코드에 큰따옴표로 직접 적어 넣은 문자열이다. (`"id,name,dept,salary"`)

- 끝에 눈에 보이지 않는 `'\0'`이 자동으로 붙는다. (`sizeof("ABC") == 4`)
- 실행 중에 만들어지는 문자열(`sprintf`로 채운 버퍼, `strdup`의 결과 등)은 리터럴이 아니다.
- C 표준상 **정적 저장 기간(static storage duration)** 을 가진다. 컴파일 시점에 내용이 정해져 있으므로 실행 파일 안(보통 `.rodata`, 읽기 전용 섹션)에 들어가고, 프로그램이 끝날 때까지 유지된다.
  - 스택도 힙도 아니므로 `free()` 할 수 없다.
  - 수정하면 UB이며, 보통 읽기 전용 페이지라서 세그폴트가 난다. 수정이 필요하면 먼저 복사해야 한다.

> `const`가 붙어서 정적 메모리에 있는 게 아니다. 저장 위치는 **어떻게 만들었는지**(리터럴 / 지역 변수 / `malloc` / `static`·전역)가 결정하고, `const`는 **그 포인터로 수정할 수 있는지**만 결정한다.

<br/>

### 2-3. `strdup`: 힙에 복사본 만들기

```c
static void parse_row(Row *r, const char *csv) {
    r->base = strdup(csv);   // 힙에 복사본을 만들고 그 주소를 반환
    if (!r->base) { perror("strdup"); exit(1); }
    r->n = 0;

    for (char *tok = strtok(r->base, ","); tok && r->n < MAX_FIELDS;
         tok = strtok(NULL, ",")) {
        r->fields[r->n++] = tok;   // 현재 n을 인덱스로 쓰고, 그다음 n을 1 증가
    }
}
```

`strdup`은 내부에서 `malloc` + 복사를 한 번에 해준다. 따라서 따로 `malloc`할 필요는 없고, 다 쓰고 나면 **반환받은 포인터를 `free`** 해야 한다.

<br/>

### 2-4. `strtok`은 원본을 직접 수정한다 ⭐

처음에는 `strtok`이 토큰을 잘라서 **새 문자열을 만들어** 돌려주는 줄 알았다. 하지만 실제로는 구분자 자리를 `'\0'`으로 덮어써서 토큰을 끝내고, **원본 안을 가리키는 포인터**를 돌려줄 뿐이다.

```
처음:    [i][d][,][n][a][m][e][,][d][e][p][t][,]...
호출 후: [i][d][\0][n][a][m][e][\0][d][e][p][t][\0]...
          ↑ 1번째     ↑ 2번째        ↑ 3번째
```

`fields`는 포인터 배열일 뿐이고, 문자열 데이터가 따로 복사되지 않는다. 즉 모든 `fields[i]`는 `r->base`라는 **하나의 힙 블록 내부**를 가리킨다.

```
r->base ──► [i][d][\0][n][a][m][e][\0][d][e][p][t][\0][s][a][l][a][r][y][\0]
             ↑          ↑              ↑              ↑
          fields[0]  fields[1]      fields[2]      fields[3]
```

<br/><br/>


### 2-5. 그래서 무엇이 잘못됐나

`free()`에는 할당 함수가 **반환한 바로 그 주소**를 넘겨야 한다.

| 호출 | 결과 |
|---|---|
| `free(fields[0])` | `fields[0] == r->base`라서 우연히 성공 → 블록 전체 해제 |
| `free(fields[1])` | 블록 **중간** 주소 → `free(): invalid pointer` → `SIGABRT` |

<br/><br/>

## 3. 해결

`strdup`이 반환한 `r->base`만 **한 번** 해제한다.

```c
static void row_free(Row *r) {
    free(r->base);     // strdup이 반환한 바로 그 주소
    r->base = NULL;    // 댕글링 포인터 방지 (중복 호출 시 free(NULL)이라 안전)
    r->n = 0;
}
```

- 해제 후에는 `fields[i]`들도 전부 무효 포인터가 된다.
- `free(r->fields[0])`로 대신하면 안 된다. 입력이 `""`나 `",,,"`이면 토큰이 없어 `fields[0]`이 쓰레기 값이고(`base`는 누수), `",id"`처럼 쉼표로 시작하면 `fields[0] == base + 1`이 되어 또 invalid pointer가 된다.

<br/><br/>

## 4. 추가로 궁금했던 점

> ❓ `csv`는 정적 메모리의 리터럴을, `r->base`는 `strdup`으로 만든 힙 메모리를 가리킨다. 그렇다면 둘은 같은 원본이 아니라 서로 다른 데이터를 가리키는 걸까?

<br/>

💡 **맞다.** 같은 내용의 문자열이 두 군데에 따로 존재하고, 두 포인터는 서로 다른 주소를 가리킨다.

```
정적 영역 (읽기 전용)
csv ──────► [i][d][,][n][a][m][e][,][d][e][p][t][,][s][a][l][a][r][y][\0]   ← 원본 리터럴

힙
r->base ──► [i][d][,][n][a][m][e][,][d][e][p][t][,][s][a][l][a][r][y][\0]   ← strdup 복사본
```

파싱 후에는 차이가 분명해진다. `strtok`은 힙 복사본만 수정한다.

```
csv     ──► [i][d][,][n][a][m][e][,]...     ← 변함없음
r->base ──► [i][d][\0][n][a][m][e][\0]...   ← 여기만 잘림
```

수명도 다르다. 힙 복사본은 `free()` 하면 사라지지만, 리터럴은 프로그램이 끝날 때까지 남아 있다. `strdup`으로 복사본을 만든 이유가 바로 **수정 불가능한 리터럴을 건드리지 않고 안전하게 자르기 위해서**다.

> 참고: `csv`라는 **포인터 변수 자체**는 함수 매개변수라 스택에 있고, 그 변수에 담긴 **주소 값**이 정적 영역을 가리킨다. `r->base`도 변수는 `main` 스택의 `Row r` 안에 있고, 담긴 주소가 힙을 가리킨다.


<br/><br/>

## 5. 정리

- `free()`에는 `malloc`/`strdup` 등이 **반환한 포인터 그대로**를 넘긴다. 블록 중간 주소, 스택 주소, 리터럴 주소는 모두 UB다.
- 문자열 리터럴은 정적(읽기 전용) 영역에 있어서 `free`도 수정도 불가능하다. 수정하려면 복사본을 만든다.
- `strtok`은 새 문자열을 만들지 않고 원본에 `'\0'`을 써 넣는다. 토큰 포인터들은 모두 원본 버퍼 안을 가리킨다.
- 한 번 할당했으면 한 번 해제한다. 해제 후에는 포인터를 `NULL`로 두면 안전하다.
- 이런 버그는 `-fsanitize=address`(AddressSanitizer)나 Valgrind로 빠르게 찾을 수 있다.