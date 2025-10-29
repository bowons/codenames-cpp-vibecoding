#include <iostream>
#include "../../include/core/IOCPClient_2.h"


IOCPClient_2::IOCPClient_2()
: socket_(INVALID_SOCKET),
    iocpHandle_(NULL),
    connected_(false),
    workerThread_(nullptr)
{
}

IOCPClient_2::~IOCPClient_2() {
    Close();
}

bool IOCPClient_2::Initialize() {

    // Winsock 라이브러리 초기화
    WSADATA wsaData;

    // Winsock DLL 버전 2.2 요청 초기화
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        return false;
    }

    // 초기화 수행 후 실패시 false 변환
    if (!InitializeIOCP()) {
        WSACleanup();
        return false;
    }

    return true;
}

void IOCPClient_2::Close()
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
}

bool IOCPClient_2::Connect(const std::string& ip, int port) {
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;

    // inet_pton으로 아이피 주소 바이너리로 변환
    // p - text presentation을 의미
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) != 1) {
        std::cerr << "Invalid IP address format: " << ip << std::endl;
        return false;
    }

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

void IOCPClient_2::Disconnect() {
    // connected false
    connected_ = false;

    if (socket_ != INVALID_SOCKET) {
        // 소켓 닫기
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }

    if (onDisconnected) {
        onDisconnected();
    }

}

bool IOCPClient_2::SendData(const std:: string& data) {
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

    auto ctx = new IOContext();
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

    return true;
}

bool IOCPClient_2::InitializeIOCP() {
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
    workerThread_ = std::make_unique<std::thread>(&IOCPClient_2::WorkerThread, this);

    return true;
}

void IOCPClient_2::CleanupIOCP() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }

    if (iocpHandle_) {
        CloseHandle(iocpHandle_);
        iocpHandle_ = NULL;
    }
}

void IOCPClient_2::WorkerThread() {
    DWORD byteTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    while (connected_) {
        // loop 돌면서 완료된 IO 가져옴
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

void IOCPClient_2::ProcessReceivedData(const std::string& data) {
    if (onDataReceived) {
        onDataReceived(data);
    }
}