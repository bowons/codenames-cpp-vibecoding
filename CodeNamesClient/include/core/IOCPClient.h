#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <memory>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

class IOCPClient {
public:
    static constexpr int BUFFER_SIZE = 256;
    static constexpr int SERVER_PORT = 55014;

    IOCPClient();
    ~IOCPClient();

    bool Initialize();
    void Close();
    bool Connect(const std::string& ip, int port);
    void Disconnect();
    bool SendData(const std::string& data);
    bool IsConnected() const { return connected_; }

    std::function<void(const std::string&)> onDataReceived;  // 데이터 수신 콜백
    std::function<void()> onConnected;
    std::function<void()> onDisconnected;

private:
    bool InitializeIOCP();
    void CleanupIOCP();
    void WorkerThread();
    void ProcessReceivedData(const std::string& data);

    SOCKET socket_;
    HANDLE iocpHandle_;
    std::atomic<bool> connected_;
    std::unique_ptr<std::thread> workerThread_;

    struct IOContext {
        OVERLAPPED overlapped;
        WSABUF wsaBuf;
        char buffer[BUFFER_SIZE];
        int operation; // 0 = RECV, 1 = SEND
    };
};