#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

#include "IMediator.h"

class Session; // Session.h를 include하지 않고 포인터만 사용
class IOCPServer;
class SessionManager;

class NetworkManager {
    public:
        NetworkManager(int port, IMediator* server);
        ~NetworkManager();

        void SetServer(IMediator* server) { server_ = server; }
        IMediator* GetServer() const { return server_; }

        bool Initialize();
        void Shutdown();

        bool AssociateSocketWithIOCP(SOCKET socket, Session* session);
        void RemoveSession(SOCKET socket);

        SOCKET CreateListenSocket(int port);
        bool StartAccept();

    private:
        IMediator* server_;  // IMediator 참조

        HANDLE iocpHandle_;
        SOCKET listenSocket_;
        
        // 스레드 관리
        std::vector<std::thread> workerThreads_; // IOCP에서 완료된 작업들을 처리하는 스레드
        bool isRunning_;
        int workerThreadCount_;

        std::thread acceptThread_; // Accept 전용 스레드

    private:
        bool CreateIOCP();
        void CreateWorkerThreads(int count);
        void WorkerThreads();
        void ProcessCompletionPacket(DWORD bytesTransferred, Session* session,
                                 struct OverlappedEx* overlapped);
        void HandleNewClient(SOCKET clientSocket, const sockaddr_in& clientAddr);
};