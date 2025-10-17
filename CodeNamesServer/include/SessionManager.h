#pragma once

#include <winsock2.h>
#include <unordered_map>
#include <queue>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <unordered_map>

class Session;
class IOCPServer;

class SessionManager { // 기존 프로젝트의 RoomManager 대체
private:
    IOCPServer* server_; 
    
    std::unordered_map<SOCKET, std::shared_ptr<Session>> sessions_;
    std::unordered_map<std::string, SOCKET> tokenToSocket_; // 토큰 + 소켓 매핑
    std::queue<SOCKET> matchingQueue_; //매칭 대기열
    std::mutex sessionMutex_;
    std::atomic<size_t> sessionCount_;

public:
    SessionManager(IOCPServer* server);
    ~SessionManager();

    // 세션 관리
    bool AddSession(std::shared_ptr<Session> session);
    void RemoveSession(SOCKET socket); // 검색으로 인한 오버헤드 최소화를 위해 소켓으로 제거
    
    // 세션 조회 및 토큰 검증
    std::shared_ptr<Session> FindSession(SOCKET socket);
    std::shared_ptr<Session> FindSessionByToken(const std::string& token);
    bool ValidateToken(const std::string& token);

    // 매칭 대기열 관리
    std::vector<std::shared_ptr<Session>> GetWaitingPlayers();
    bool AddToMatchingQueue(std::shared_ptr<Session> session);
    void RemoveFromMatchingQueue(std::shared_ptr<Session> session);
    void RequestGameRoomCreation(const std::vector<std::shared_ptr<Session>>& players);

    // 로비/매칭 패킷 처리
    void HandleLobbyPacket(class Session* session, const std::string& data);

    // 인증 패킷 처리 (CHECK_ID, SIGNUP, LOGIN, TOKEN, EDIT_NICK)
    void HandleAuthProtocol(class Session* session, const std::string& data);

    // 브로드캐스트 기능(프로젝트에서는 사용 안함)
    // #comment - GPT는 이 기능이 실제 서비스에서 공지나 점검 등으로 사용될 수 있다고 한다, 학습용으로 추가
    void BroadcastToAll(const std::string& message);

    // 통계 및 관리
    size_t GetSessionCount() const;
    void DisconnectAll();
};