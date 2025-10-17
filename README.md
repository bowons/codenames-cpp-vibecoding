# CodeNames C++ IOCP

Windows IOCP를 기반으로 구현한 실시간 멀티플레이 CodeNames 게임 서버 및 클라이언트입니다.
C++17과 Winsock2를 활용하여 비동기 네트워크 통신을 구현했으며, SQLite 데이터베이스를 통해 사용자 인증 및 게임 데이터를 관리합니다.

**이 프로젝트는 Claude AI(Sonnet, Haiku)를 사용한 Vibe-Coding로 진행되었습니다.**

---

## 주요 구현 기능

• **IOCP 기반 비동기 네트워크 통신**
  ◦ Windows IOCP를 활용한 고성능 멀티스레드 서버
  ◦ 동시 다중 클라이언트 연결 처리 (4명 동시 게임 매칭)
  ◦ 워커 스레드 풀 기반 이벤트 처리

• **사용자 인증 시스템**
  ◦ SQLite 데이터베이스 기반 계정 관리
  ◦ 회원가입, 로그인, 토큰 기반 세션

• **게임 매칭 및 세션 관리**
  ◦ 플레이어 매칭 큐 시스템
  ◦ 4명 모이면 자동 게임 시작
  ◦ 팀 분배 및 역할 자동 할당 (스파이마스터/요원)

• **실시간 게임 동기화**
  ◦ 턴 기반 게임 상태 업데이트
  ◦ 카드 공개 상태 실시간 동기화
  ◦ 게임 내 채팅 기능

• **Observer 패턴 기반 UI 동기화**
  ◦ GameState 변경 시 자동으로 모든 UI 업데이트
  ◦ 9개 게임 상태 콜백 함수

• **Windows Console 기반 UI**
  ◦ 5개 화면 구현 (로그인, 메뉴, 매칭, 게임, 결과)
  ◦ 컬러 텍스트, 5x5 카드 그리드

• **프로토콜 호환성**
  ◦ 50+ 게임 프로토콜 정의 및 검증
  ◦ 클라이언트-서버 완벽 호환

---

## 기술 스택

**Language:** C++17  
**Networking:** Windows IOCP, Winsock2  
**Database:** SQLite3  
**UI:** Windows Console API  
**Build:** CMake, vcpkg  
**Compiler:** MSVC 19.36 (Visual Studio 2022)  
**Platform:** Windows x64

---

## Vibe Coding 협업 과정

이 프로젝트는 **Vibe Coding** 방식으로 진행된 협업 프로젝트입니다. 각 단계별로 AI와의 상호작용을 통해 개발되었습니다:

### 아키텍처 설계 (bowons)
게임 서버를 **Manager 계층으로 구조화**하여 NetworkManager, SessionManager, DatabaseManager, GameManager로 분할했습니다.
각 모듈 간 **Mediator 패턴**과 **Observer 패턴** 등 디자인 패턴을 적용하여 느슨한 결합 구조를 설계했습니다.
50+ 개의 게임 프로토콜을 정의하고 클라이언트-서버 간 완벽한 호환성을 검증했습니다.

### 서버 구현 (Claude 4 Sonnet)
bowons이 설계한 Manager 기반 아키텍처를 구현했습니다:
- **NetworkManager**: Windows IOCP API, CreateIoCompletionPort, WSAAccept 등 IOCP 기반 비동기 I/O 구현
- **SessionManager**: 설계된 프로토콜 핸들러들을 Mediator 패턴에 따라 구현
- **DatabaseManager**: SQLite 통합 및 쿼리 작성
- **GameManager**: Mediator 패턴을 통한 각 Manager 간 조율 로직 구현

IOCP와 Windows 비동기 API를 기반으로 한 **비동기 네트워크 통신**의 복잡한 부분을 담당했습니다.

### 클라이언트 개발 (Claude 4.5 Haiku)
bowons의 요청 기반으로 클라이언트 전체를 구현했습니다:
- **IOCPClient**: Windows IOCP 기반 비동기 네트워킹 클라이언트
- **GUIManager**: 5개 게임 화면 관리 및 상태 전환
- **GameScreen**: 게임 UI 구현 및 Observer 패턴을 통한 상태 동기화
- 20+ 개의 프로토콜 파서 및 패킷 핸들러 작성

