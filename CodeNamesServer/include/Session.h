#pragma once

#include <winsock2.h>
#include <Windows.h>
#include <memory>
#include <string>
#include <mutex>

// IOCP 작업 종류
enum class IOOperation {
    ACCEPT,
    RECV,
    SEND
};

// Session 상태
enum class SessionState {
    AUTHENTICATING,    // CHECK_ID, SIGNUP, LOGIN, TOKEN 처리
    WAITING_MATCH,     // CMD|QUERY_WAIT, SESSION_READY, MATCHING_CANCEL 처리  
    IN_LOBBY,         // LOBBY 관련 패킷 처리
    IN_GAME           // CHAT, HINT, ANSWER, REPORT 처리
};

// 버퍼 크기 상수
constexpr int SESSION_BUFFER_SIZE = 4096;

// IOCP 사용 확장 구조체
struct OverlappedEx : public OVERLAPPED {
    IOOperation operation; // 작업
    WSABUF wsaBuf; // 소켓 버퍼
    char buffer[SESSION_BUFFER_SIZE];
    class Session* session; // 세션 포인터

    OverlappedEx(IOOperation op, Session* sess = nullptr) : operation(op), session(sess) {
        // OVERLAPPED는 Windows 구조체이므로 ZeroMemory가 안전하다고 함
        ZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED));
        ZeroMemory(buffer, sizeof(buffer));
        wsaBuf.buf = buffer;
        wsaBuf.len = sizeof(buffer);
    }
};

struct UserInfo {
    std::string id;
    std::string nickname;
    int wins = 0;
    int losses = 0;
    int report_count = 0;
    bool is_suspended = false;
};

class Session : public std::enable_shared_from_this<Session> {
private:
    SOCKET socket_;
    class NetworkManager* networkManager_;

    // 매니저 참조 (소유권 없음, 단순 참조)
    class GameManager* gameManager_;      // 게임방별 고유 매니저
    class UserManager* userManager_;     // 게임방별 고유 매니저 (추후 결정)
    class IOCPServer* server_;           // 서버 참조 (전역 매니저들 접근용)

    // 세션 상태
    std::string token_; // 인증 토큰
    std::string username_; // 닉네임
    bool isInMatchingQueue_; // 매칭 대기 여부
    SessionState currentState_; // 세션 상태

    // 버퍼 및 뮤텍스
    std::mutex sendLock_;
    bool isClosed_;

    // 로그인 후 UserInfo
    UserInfo userInfo_;
    bool isLoggedIn_ = false;

public:
    Session(SOCKET sock, class NetworkManager* networkManager);
    ~Session();

    // 세션 초기화/종료
    bool Initialize();
    void Close();

    // 네트워크 작업
    bool PostRecv();
    bool PostSend(const std::string& data);
    void ProcessRecv(size_t bytesTransferred);
    void ProcessSend(size_t bytesTransferred);

    // 상태 접근자
    bool IsAuthenticated() const { return !token_.empty(); }
    const std::string& GetUserName() const { return username_; }
    void SetUserName(const std::string& name) { username_ = name; }
    void SetToken(const std::string& token) { token_ = token; }
    void SetState(SessionState state) { currentState_ = state; }

    // 로그인
    bool Login(const std::string& id, const std::string& pw); // DB 조회 후 userInfo_ 설정
    const UserInfo& GetUserInfo() const { return userInfo_;}
    void SetUserInfo(const UserInfo& info) { userInfo_ = info; }
    void SetLoggedIn(bool loggedIn) { isLoggedIn_ = loggedIn; }

    SOCKET GetSocket() const { return socket_; }
    SessionState GetState() const { return currentState_; }
    bool IsClosed() const { return isClosed_; }
    const std::string& GetToken() const { return token_; }    

    void SetInMatchingQueue(bool inQueue) { isInMatchingQueue_ = inQueue; }
    bool IsInMatchingQueue() const { return isInMatchingQueue_; }

    // Manager Setter/Getter
    void SetGameManager(class GameManager* gm) { gameManager_ = gm; }
    void SetUserManager(class UserManager* um) { userManager_ = um; }
    void SetServer(class IOCPServer* server) { server_ = server; }

    // 고유 매니저
    class GameManager* GetGameManager() const { return gameManager_; }
    class UserManager* GetUserManager() const { return userManager_; }
    
    // 전역 매니저들은 서버를 통해 접근
    class NetworkManager* GetNetworkManager() const;
    class DatabaseManager* GetDatabaseManager() const;
    class SessionManager* GetSessionManager() const;

    // Server 사용 헬퍼 함수
    template<typename Func>
    bool WithServer(Func&& func) const {
        if (server_) {
            return func(server_);
        }
        return false;
    }

};

