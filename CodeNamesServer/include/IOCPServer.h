#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <unordered_map>

class IOCPServer {
public:
    static constexpr int BUFFER_SIZE = 256;
    static constexpr int MAX_CLIENTS = 64;
    static constexpr int TOKEN_LEN = 64;    
    static constexpr int SERVER_PORT = 55014;
    static constexpr int TCP_PORT = 55015;

public:
    IOCPServer(int port = SERVER_PORT);
    ~IOCPServer();

    bool Initialize();
    void Start();
    void Stop();

private:
    bool isRunning;
    int port;
    
    std::unique_ptr<class NetworkManager> networkManager_;
    std::unique_ptr<class SessionManager> sessionManager_;

    // 복사 방지
    IOCPServer(const IOCPServer&) = delete;
    IOCPServer& operator=(const IOCPServer&) = delete;

public:
    void RemoveSession(SOCKET socket);
    bool AddSession(std::shared_ptr<class Session> session);
    
    // 매니저 접근자들 (DatabaseManager는 싱글톤으로 직접 접근)
    SessionManager* GetSessionManager() const { return sessionManager_.get(); }
    NetworkManager* GetNetworkManager() const { return networkManager_.get(); }
    
    void CreateGameRoom(const std::vector<std::shared_ptr<class Session>>& players);
    void RemoveGameRoom(const std::string& roomId);

private:
    std::unordered_map<std::string, std::unique_ptr<class GameManager>> activeGames_;
    std::mutex gamesMutex_;  // activeGames_ 보호용
}; 