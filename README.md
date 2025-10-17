# CodeNames C++ IOCP Implementation

**진행 상황: 🎉 완성 (Build Success)**

코드네임즈 게임을 C++17과 Windows IOCP를 이용하여 구현한 프로젝트입니다.

## 📋 프로젝트 개요

- **클라이언트**: Windows Console UI (GUIManager)
- **서버**: IOCP 기반 비동기 멀티플레이 게임 서버
- **통신**: TCP/IP (포트 55014)
- **데이터베이스**: SQLite3
- **프로토콜**: 파이프(|) 구분자 기반 텍스트 프로토콜

## 🏗️ 프로젝트 구조

```
CodeNamesIOCP/
├── CodeNamesClient/
│   ├── include/
│   │   ├── core/
│   │   │   ├── IOCPClient.h
│   │   │   ├── PacketHandler.h
│   │   │   ├── PacketProtocol.h
│   │   │   └── GameState.h
│   │   └── gui/
│   │       ├── GUIManager.h
│   │       ├── LoginScreen.h
│   │       ├── GameScreen.h
│   │       ├── MainScreen.h
│   │       ├── ResultScreen.h
│   │       └── ConsoleUtils.h
│   ├── src/
│   │   ├── main.cpp
│   │   ├── core/
│   │   └── gui/
│   ├── build/
│   └── CMakeLists.txt
│
├── CodeNamesServer/
│   ├── include/
│   │   ├── IOCPServer.h
│   │   ├── NetworkManager.h
│   │   ├── Session.h
│   │   ├── SessionManager.h
│   │   ├── DatabaseManager.h
│   │   ├── GameManager.h
│   │   └── ...
│   ├── src/
│   │   ├── main.cpp
│   │   ├── IOCPServer.cpp
│   │   ├── NetworkManager.cpp
│   │   ├── Session.cpp
│   │   ├── SessionManager.cpp
│   │   ├── DatabaseManager.cpp
│   │   └── GameManager.cpp
│   ├── build/
│   └── CMakeLists.txt
│
└── vcpkg/ (의존성 관리)
```

## 🔧 빌드 환경

- **컴파일러**: MSVC 19.36 (Visual Studio 2022)
- **C++ 표준**: C++17
- **플랫폼**: Windows x64
- **SDK**: Windows 10.0.26100.0

### 의존성

- **sqlite3**: vcpkg에서 설치
- **Winsock2**: Windows 기본 라이브러리
- **IOCP**: Windows 비동기 I/O

## 🚀 빌드 및 실행 방법

### 1. vcpkg 설정

```bash
cd vcpkg
.\vcpkg.exe install sqlite3:x64-windows
```

### 2. 클라이언트 빌드

```bash
cd CodeNamesClient
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="../../vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Debug
```

**실행:**
```bash
.\Debug\CodeNamesClient.exe [server_ip] [server_port]
# 기본값: 127.0.0.1:55014
```

### 3. 서버 빌드

```bash
cd CodeNamesServer
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="../../vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Debug
```

**실행:**
```bash
.\Debug\CodeNamesServer.exe
```

## 📡 네트워크 프로토콜

### 인증 프로토콜

| 요청 | 응답 | 설명 |
|------|------|------|
| `CHECK_ID\|{id}` | `ID_TAKEN` / `ID_AVAILABLE` | 아이디 중복 검사 |
| `SIGNUP\|{id}\|{pw}\|{nick}` | `SIGNUP_OK\|{token}` | 회원가입 |
| `LOGIN\|{id}\|{pw}` | `LOGIN_OK\|{token}` | 로그인 |
| `TOKEN\|{token}` | `TOKEN_VALID\|{nick}` | 토큰 검증 |

### 게임 프로토콜

| 요청 | 응답 | 설명 |
|------|------|------|
| `CMD\|QUERY_WAIT\|{token}` | `WAIT_REPLY\|{count}` | 매칭 대기 |
| `SESSION_READY\|{token}` | `SESSION_ACK` | 세션 준비 |
| `MATCHING_CANCEL\|{token}` | `CANCEL_OK` | 매칭 취소 |
| `HINT\|{word}\|{count}` | `HINT\|{team}\|{word}\|{count}` | 힌트 제공 |
| `ANSWER\|{word}` | `CARD_UPDATE\|{idx}\|{used}` | 카드 선택 |
| `CHAT\|{message}` | `CHAT\|{team}\|{role}\|{nick}\|{msg}` | 채팅 |

## 🔌 IOCP 아키텍처

### 클라이언트 (IOCPClient)

```
Main Thread
    ↓
[IOCPClient]
    ├─→ WSAConnect()
    ├─→ WorkerThread (IOCP 완료 대기)
    │   └─→ WSARecv (데이터 수신)
    │   └─→ OnDataReceived() 콜백
    └─→ WSASend (데이터 전송)
```

### 서버 (IOCPServer)

```
Main Thread
    ↓
[IOCPServer]
    ├─→ NetworkManager
    │   ├─→ CreateIoCompletionPort()
    │   ├─→ CreateListenSocket() + WSAAccept()
    │   ├─→ WorkerThreads (IOCP 완료 대기)
    │   │   ├─→ ProcessRecv() 콜백
    │   │   ├─→ ProcessSend() 콜백
    │   │   └─→ SessionManager
    │   │       ├─→ HandleAuthProtocol()
    │   │       ├─→ HandleGameProtocol()
    │   │       └─→ DatabaseManager (SQLite)
    │   └─→ SessionManager (클라이언트 세션 관리)
    └─→ GameManager (게임 상태 관리)
```

## 🎮 게임 플로우

1. **로그인 화면**
   - 아이디 검증 (CHECK_ID)
   - 회원가입 또는 로그인
   - 토큰 획득

2. **로비 화면**
   - 프로필 표시 (승률)
   - 게임 시작 버튼

3. **매칭 화면**
   - 4명의 플레이어 대기
   - 대기 중인 플레이어 수 표시

4. **게임 화면**
   - 5x5 카드 그리드
   - 팀별 힌트 및 정답 입력
   - 실시간 채팅

5. **결과 화면**
   - 게임 결과 표시
   - 승패 기록

## ✅ 호환성 검사 결과

📋 상세 보고서: `COMMUNICATION_COMPATIBILITY_CHECK.md`

| 항목 | 상태 |
|------|------|
| 포트 설정 | ✅ 일치 (55014) |
| 인증 프로토콜 | ✅ 완전 호환 |
| 게임 프로토콜 | ✅ 완전 호환 |
| 네트워크 구성 | ✅ 동일 |

## 🐛 알려진 문제

- 없음 (모든 호환성 문제 해결됨)

## 📝 최근 수정 사항

### 2025-10-17

- ✅ 클라이언트 포트 설정 (55015 → 55014)
- ✅ 서버 Winsock 헤더 순서 정정 (WIN32_LEAN_AND_MEAN)
- ✅ LoginScreen ENTER 키 버그 수정
- ✅ GameScreen 콜백 구현 완료
- ✅ 패킷 호환성 검사 및 수정
  - `LOGIN_WRONG_PASSWORD` → `LOGIN_WRONG_PW`
  - `TOKEN_INVALID` → `INVALID_TOKEN`
  - `READY_ACK` → `SESSION_ACK`
- ✅ 불필요한 헤더 파일 정리
- ✅ 클라이언트 & 서버 빌드 성공

## 📚 참고 자료

- [Microsoft IOCP 문서](https://learn.microsoft.com/en-us/windows/win32/fileio/completion-ports)
- [Winsock2 API 레퍼런스](https://learn.microsoft.com/en-us/windows/win32/winsock/winsock-reference)
- [C++ Networking Best Practices](https://isocpp.org/)

## 👤 개발자

- **bowons** (qh0614dd@knu.ac.kr)

## 📄 라이센스

This project is part of educational work.

---

**마지막 업데이트**: 2025-10-17
**상태**: ✅ 빌드 성공, 호환성 완벽
