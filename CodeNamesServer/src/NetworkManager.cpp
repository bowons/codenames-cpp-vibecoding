#include "IOCPServer.h"
#include "NetworkManager.h"
#include "SessionManager.h"
#include "Session.h"
#include <winsock2.h>
#include <mswsock.h>
#include <iostream>
#include <thread>
#include <stdexcept>

NetworkManager::NetworkManager(int port, IMediator* server)
    : iocpHandle_(nullptr), listenSocket_(INVALID_SOCKET),
        isRunning_(false), workerThreadCount_(4),
        server_(server) {
    
    // WSAStartup 호출
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        throw std::runtime_error("WSAStartup failed");
    }

    listenSocket_ = CreateListenSocket(port);
    if (listenSocket_ == INVALID_SOCKET) {
        WSACleanup(); // Startup 이후 실패 시 Cleanup
        throw std::runtime_error("Failed to create listen socket");
    }   
}

NetworkManager::~NetworkManager() {
    // 서버 종료 Flag
    isRunning_ = false;

    // Accept 스레드 종료 대기
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    // 스레드 종료 신호 보내기
    if (iocpHandle_) {
        for (size_t i = 0; i < workerThreads_.size(); ++i) {
            PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
        }
    }

    // 스레드 종료 로그 추가
    std::cout << "Waiting for worker threads to terminate..." << std::endl;
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join(); // 정상적으로 종료되길 기대
        }
    }
    std::cout << "All worker threads terminated" << std::endl;

    // 리소스 정리
    if (listenSocket_ != INVALID_SOCKET) {
        closesocket(listenSocket_);
    }   

    if (iocpHandle_) {
        CloseHandle(iocpHandle_);
    }

    WSACleanup();
}

void NetworkManager::RemoveSession(SOCKET socket) {
    if (server_) {
        server_->RemoveSession(socket);
    }
}

bool NetworkManager::Initialize() {
    // IOCP 생성
    if (!CreateIOCP()) {
        std::cerr << "Failed to create IOCP" << std::endl;
        return false;
    }

    // Worker Thread 생성
    CreateWorkerThreads(workerThreadCount_);

    // Accept 시작
    // if (!StartAccept()) {
    //     std::cerr << "Failed to start accept" << std::endl;
    //     return false;
    // }

    // 서버 가동상태 변경
    isRunning_ = true;
    
    std::cout << "NetworkManager initialized successfully" << std::endl;
    return true;
}

void NetworkManager::Shutdown() {
    isRunning_ = false;
    
    if (listenSocket_ != INVALID_SOCKET) {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }
    
    std::cout << "NetworkManager shutdown complete" << std::endl;
}

bool NetworkManager::CreateIOCP()
{
    iocpHandle_ = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, // 기존 파일 핸들 (소켓 연결용이 아닌 새로 생성)
        NULL,                 // 기존 IOCP 핸들 (새로 생성하므로 NULL)
        0,                    // CompletionKey (새로 생성하므로 0)
        0                     // 동시 실행 스레드 수 (0 = 시스템이 결정)
    );

    if (iocpHandle_ == NULL) 
    {
        std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << std::endl;
        return false;
    }

    std::cout << "IOCP created successfully" << std::endl;
    return true;
}

void NetworkManager::CreateWorkerThreads(int count) {
    // 벡터 크기 미리 할당 (성능 최적화)
    workerThreads_.reserve(count);

    for (int i = 0; i < count; ++i) {
        workerThreads_.emplace_back(&NetworkManager::WorkerThreads, this);
    }
    

    std::cout << "Created " << count << " worker threads" << std::endl;
}

SOCKET NetworkManager::CreateListenSocket(int port) {
    // TCP 소켓 생성
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        return INVALID_SOCKET;
    }

    // SO_REUSEDADDR 옵션 설정 (포트 재사용 허용여부)
    int optval = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&optval), sizeof(optval)) == SOCKET_ERROR) {

    }

    // 주소 구조체 설정
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;    // IPv4
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // 모든 주소 허용
    serverAddr.sin_port = htons(port);  // 지정 포트

    // 바인딩
    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "bind failed on port " << port << ": " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    // 리스닝 시작
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) // SOMAXCONN: 시스템 최대값 사용
    {
        std::cerr << "listen failed on port " << port << ": " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    std::cout << "Listen socket created on port " << port << std::endl;
    return listenSocket;
}

void NetworkManager::HandleNewClient(SOCKET clientSocket, const sockaddr_in& clientAddr) {
    // Session 생성
    auto session = std::make_shared<Session>(clientSocket, this);

    // IOCP에 소켓 연결
    if (!AssociateSocketWithIOCP(clientSocket, session.get())) {
        std::cerr << "Failed to associate client socket with IOCP" << std::endl;
        closesocket(clientSocket);
        return;
    }

    // SessionManager에 Session 등록
    if (!server_->AddSession(session)) {
        std::cerr << "Failed to add session to SessionManager" << std::endl;
        closesocket(clientSocket);
        return;
    }

    // 첫 번째 Recv 요청
    if (!session->Initialize()) {
        std::cerr << "Failed to initialize session for new client" << std::endl;
        session->Close();
        return;
    }

    // LOG
    std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr) 
              << ":" << ntohs(clientAddr.sin_port) << std::endl;
}

bool NetworkManager::StartAccept() {
    // Accept 전용 스레드 생성

    acceptThread_ = std::thread([this]() {
        while (isRunning_) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);

            // 클라이언트 연결 대기
            SOCKET clientSocket = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
            if (clientSocket == INVALID_SOCKET) {
                if (isRunning_) {
                    std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
                }
                continue;
            }

            // 새 클라이언트 처리
            HandleNewClient(clientSocket, clientAddr);
        }
    });

    std::cout << "Accept thread started" << std::endl;
    return true;
}

bool NetworkManager::AssociateSocketWithIOCP(SOCKET socket, Session* session) {
    // Client 소켓을 생성된 IOCP에 연결
    HANDLE result = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(socket), // 소켓 핸들
        iocpHandle_,                      // 기존 IOCP 핸들
        reinterpret_cast<ULONG_PTR>(session), // CompletionKey로 Session 포인터 전달
        0                                 // 동시 실행 스레드 수 (0 = 시스템이 결정)
    );

    if (result != iocpHandle_) {
        std::cerr << "CreateIoCompletionPort (associate) failed: " << GetLastError() << std::endl;
        return false;
    }

    return true;   
}

void NetworkManager::WorkerThreads() {
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    while (isRunning_) {
        // IOCP에서 완료된 소켓 I/O 작업 가져오기
        BOOL result = GetQueuedCompletionStatus(
            iocpHandle_,               // IOCP 핸들
            &bytesTransferred,         // 전송된 바이트 수
            &completionKey,           // CompletionKey (Session* 포인터)
            &overlapped,              // OVERLAPPED 구조체
            INFINITE                  // 대기 시간 (무한대기)
        );

        // 종료 신호 확인 (소멸자에서 PostQueuedCompletionStatus(0, 0, nullptr) 호출)
        if (!result && overlapped == nullptr) {
            std::cout << "Worker thread terminating..." << std::endl;
            break;
        }

        // Session과 OverlappedEx 포인터 가져오기
        auto session = reinterpret_cast<Session*>(completionKey);
        auto overlappedEx = reinterpret_cast<OverlappedEx*>(overlapped);

        if (!result) {
            // I/O 에러 발생 시
            DWORD error = GetLastError();
            std::cerr << "I/O operation failed: " << error << std::endl;

            if (session) { // 세션 Close
                session->Close();
            }

            if (overlappedEx) { // OverlappedEx 메모리 해제
                delete overlappedEx;
            }
            continue;
        }

        // 완료 패킷 처리
        ProcessCompletionPacket(bytesTransferred, session, overlappedEx);
    }
    
}

void NetworkManager::ProcessCompletionPacket(DWORD bytesTransferred, Session* session,
                                 struct OverlappedEx* overlapped) {
    
    if (!session || !overlapped) {
        if (overlapped) {
            delete overlapped; // 메모리 해제
        }
        return;
    }

    // I/O 작업 종류에 따른 처리
    switch (overlapped->operation) {
    case IOOperation::RECV:
        // Pass overlapped buffer to Session so it can read the received bytes
        session->ProcessRecv(bytesTransferred, overlapped);
        break;
    
    case IOOperation::SEND:
        session->ProcessSend(bytesTransferred);
        break;
    
    case IOOperation::ACCEPT:
        // AcceptEx 사용 시 처리 (사용하지 않음)
        break;

    default:
        std::cerr << "Unknown I/O operation" << std::endl;
        break;
    }

    // OverlappedEx 메모리 해제
    delete overlapped;
}