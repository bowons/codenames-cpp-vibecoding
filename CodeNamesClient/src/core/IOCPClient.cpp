#include <iostream>
#include "../../include/core/IOCPClient.h"

IOCPClient::IOCPClient()
    : socket_(INVALID_SOCKET),        // 소켓 초기화
      iocpHandle_(NULL),              // IOCP 핸들 초기화
      connected_(false),              // 연결 상태 초기화
      workerThread_(nullptr)          // 워커 스레드 초기화
{
}

IOCPClient::~IOCPClient() {
    Close();
}

bool IOCPClient::Initialize() {
    // Winsock 라이브러리 초기화
    WSADATA wsaData;
    // Winsock DLL 버전 2.2 요청 초기화
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    // 초기화 수행 후 실패시 false 반환
    if (!InitializeIOCP()) {
        WSACleanup();
        return false;
    }

    return true;
}

void IOCPClient::Close()
{
    // 연결 정리
    Disconnect();

    // 워커 스레드 정리
    if (workerThread_ && workerThread_->joinable()) {
        // IOCP에 종료 신호 보내기
        if (iocpHandle_) {
            PostQueuedCompletionStatus(iocpHandle_, 0, 0, NULL);
        }
        workerThread_->join();
    }

    // iocpHandle 소멸 처리
    if (iocpHandle_) {
        CloseHandle(iocpHandle_);
        iocpHandle_ = NULL;
    }

    // Winsock 정리
    WSACleanup();
}

bool IOCPClient::Connect(const std::string& ip, int port) {
    // 서버 주소 구조체 초기화
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;

    // inet_pton()으로 IP 주소를 바이너리로 변환
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1) {
        std::cerr << "Invalid IP address format: " << ip << std::endl;
        return false;
    }

    // serverAddr.sin_port = htons(port)로 포트 설정
    serverAddr.sin_port = htons(port);
        
    // connect()로 연결 시도
    int result = connect(socket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            std::cerr << "Connect failed: " << error << std::endl;
            return false;
        }
    }

    connected_ = true;
    if (onConnected) {
        onConnected();
    }

    // 첫 수신 시작
    auto recvCtx = new IOContext();
    ZeroMemory(&recvCtx->overlapped, sizeof(OVERLAPPED));
    recvCtx->operation = 0;  // RECV
    recvCtx->wsaBuf.buf = recvCtx->buffer;
    recvCtx->wsaBuf.len = BUFFER_SIZE;
    
    DWORD flags = 0;
    int recvResult = WSARecv(socket_, &recvCtx->wsaBuf, 1, NULL, &flags, &recvCtx->overlapped, NULL);
    if (recvResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << "WSARecv failed after connect: " << WSAGetLastError() << std::endl;
        connected_ = false;
        delete recvCtx;
        return false;
    }
    
    return true;
}

void IOCPClient::Disconnect() {
    // connected false 설정
    connected_ = false;
    // socket이 유효하면 closesocket() 호출
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }

    if (onDisconnected) {
        onDisconnected();
    }
}

bool IOCPClient::SendData(const std::string& data) {
    if (!connected_) {
        std::cerr << "Cannot send data: not connected" << std::endl;
        return false;
    }
    
    // 버퍼 크기 검사
    if (data.size() > BUFFER_SIZE) {
        std::cerr << "Data too large: " << data.size() << " > " << BUFFER_SIZE << std::endl;
        return false;
    }
    
    if (data.empty()) {
        std::cerr << "Empty data cannot be sent" << std::endl;
        return false;
    }
    
    // 새로운 IOContext 동적 할당
    auto ctx = new IOContext();
    ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
    ctx->operation = 1;  // SEND
    ctx->wsaBuf.buf = ctx->buffer;
    ctx->wsaBuf.len = static_cast<ULONG>(data.size());
    memcpy(ctx->buffer, data.c_str(), data.size());
    
    // WSASend 호출
    DWORD bytesSent = 0;
    int result = WSASend(
        socket_,
        &ctx->wsaBuf,
        1,
        &bytesSent,
        0,
        &ctx->overlapped,
        nullptr
    );
    
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            std::cerr << "WSASend failed: " << error << std::endl;
            delete ctx;
            return false;
        }
    }
    
    // WSA_IO_PENDING 시 ctx는 WorkerThread에서 delete (완료 후)
    return true;
}

bool IOCPClient::InitializeIOCP() {
    // iocpHandle을 CreateIoCompletionPort()로 생성
    iocpHandle_ = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,
        NULL,
        0,
        0
    );

    if (iocpHandle_ == NULL) {
        std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << std::endl;
        return false;
    }

    // WSASocket()으로 소켓 생성, IOCP에 연결함
    socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket_ == INVALID_SOCKET) {
        std::cerr << "WSASocket failed: " << WSAGetLastError() << std::endl;
        CleanupIOCP();
        return false;
    }
    
    HANDLE result = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(socket_),
        iocpHandle_,
        reinterpret_cast<ULONG_PTR>(this),
        0
    );

    if (result == NULL) {
        std::cerr << "CreateIoCompletionPort for socket failed: " << GetLastError() << std::endl;
        CleanupIOCP();
        return false;
    }

    // 워커 스레드 생성
    workerThread_ = std::make_unique<std::thread>(&IOCPClient::WorkerThread, this);

    return true;
}

void IOCPClient::CleanupIOCP()
{
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (iocpHandle_) {
        CloseHandle(iocpHandle_);
        iocpHandle_ = NULL;
    }
}

void IOCPClient::WorkerThread() {
    DWORD byteTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;
    
    // GetQueuedCompletionStatus()로 완료된 IO를 가져옴
    while (connected_) {
        BOOL result = GetQueuedCompletionStatus(
            iocpHandle_,            // IOCP 핸들
            &byteTransferred, // 전송된 바이트 수
            &completionKey,          // CompletionKey
            &overlapped,     // OVERLAPPED 구조체
            INFINITE             // 무한대기
        );

        if (!result) {
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED) {
                // 소켓이 닫혔거나 취소됨 - 정상 종료
                break;
            }
            std::cerr << "GetQueuedCompletionStatus failed: " << err << std::endl;
            break;
        }

        if (overlapped == nullptr) {
            // Close()에서 보낸 종료 신호
            break;
        }

        IOContext* context = reinterpret_cast<IOContext*>(overlapped);
        
        if (context->operation == 0) {  // RECV 완료
            if (byteTransferred == 0) {
                // 원격이 연결 종료
                std::cout << "Remote closed connection" << std::endl;
                delete context;
                break;
            }
            
            // 수신 데이터 처리
            std::string data(context->buffer, byteTransferred);
            ProcessReceivedData(data);
            
            auto nextCtx = new IOContext();
            ZeroMemory(&nextCtx->overlapped, sizeof(OVERLAPPED));
            nextCtx->operation = 0;  // RECV
            nextCtx->wsaBuf.buf = nextCtx->buffer;
            nextCtx->wsaBuf.len = BUFFER_SIZE;
            
            DWORD flags = 0;
            int recvResult = WSARecv(socket_, &nextCtx->wsaBuf, 1, NULL, &flags, &nextCtx->overlapped, NULL);
            if (recvResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                std::cerr << "WSARecv failed in loop: " << WSAGetLastError() << std::endl;
                delete nextCtx;
                delete context;
                break;
            }
            
            // 이전 컨텍스트 정리
            delete context;
            
        } else if (context->operation == 1) {  // SEND 완료
            // 전송 완료 처리
            std::cout << "Send completed: " << byteTransferred << " bytes" << std::endl;
            
            // 전송 컨텍스트 정리
            delete context;
        }
    }
}

void IOCPClient::ProcessReceivedData(const std::string& data) {
    // onDataReceived 콜백 호출 (등록된 콜백이 있으면, data 전달)

    if (onDataReceived) {
        onDataReceived(data);
    }
}