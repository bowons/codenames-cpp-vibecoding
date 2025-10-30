#include "Session.h"
#include "NetworkManager.h"
#include "SessionManager.h"
#include "GameManager.h"
#include "DatabaseManager.h"
#include "IOCPServer.h"
#include "PacketProtocol.h"

#include <iostream>
#include <ws2tcpip.h>
#include <ctime>

Session::Session(SOCKET sock, NetworkManager* networkmanager)
    : socket_(sock), networkManager_(networkmanager), server_(nullptr),
      gameManager_(nullptr), userManager_(nullptr), username_(""),
        currentState_(SessionState::AUTHENTICATING),
      isClosed_(false)
{
    std::cout << "Session 생성: 소켓 " << socket_ << std::endl;
}

Session::~Session() {
    Close();
}

bool Session::Initialize() {
    // 소켓 IOCP 등록 및 수신 요청 시작
    if (socket_ == INVALID_SOCKET) {
        std::cerr << "세션 초기화 실패: 유효하지 않은 소켓" << std::endl;
        return false;
    }   

    // IOCP 등록
    if (!networkManager_) {
        std::cerr << "NetworkManager 설정 오류" << std::endl;
        return false;
    }

    // 첫 번째 요청 수신
    if (!PostRecv()) {
        std::cerr << "초기 수신 요청 실패" << std::endl;
        return false;
    }

    return true;
}

void Session::Close() {
    if (isClosed_) return; // Close 중복 호출시 무시
    std::cout << "Session 종료: 소켓 " << socket_ << std::endl; // 종료 시점 LOG 추가
    isClosed_ = true;

    // SessionManager에서 세션 제거
    if (auto nm = GetNetworkManager()) {
        nm->RemoveSession(socket_);
    }

    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}

// IOCP 비동기 수신 요청
bool Session::PostRecv() {
    // 데이터 수신을 위한 IOCP 오버랩 구조체 생성
    auto recvOverlapped = new OverlappedEx(IOOperation::RECV);

    // WSARecv 호출
    DWORD flags = 0;
    DWORD bytesReceived = 0;

    int result = WSARecv(
        socket_,                    // 소켓
        &recvOverlapped->wsaBuf,    // 버퍼 정보
        1,                          // 버퍼 개수
        &bytesReceived,             // 받은 바이트 수
        &flags,                     // 플래그
        recvOverlapped,             // OVERLAPPED 구조체
        nullptr                     // 완료 루틴 (사용 안함)
    );

    // 결과 처리 부분
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            // 실제 에러 발생
            std::cerr << "WSARecv failed: " << error << std::endl;
            delete recvOverlapped;
            return false;
        }

        // WSA_IO_PENDING: 비동기 작업이 진행 중일 경우
    }

    return true;
}

// IOCP 비동기 송신 요청
bool Session::PostSend(const std::string& data) {
    // 데이터 크기 검사
    if (data.empty()) {
        std::cerr << "전송할 데이터가 비어있습니다." << std::endl;
        return false;
    }

    if (data.size() > SESSION_BUFFER_SIZE) {
        std::cerr << "데이터 크기가 버퍼 크기를 초과했습니다." << std::endl;
        return false;
    }

    // OverlappedEx 구조체 생성 및 데이터 복사
    auto sendOverlapped = new OverlappedEx(IOOperation::SEND);
    memcpy(sendOverlapped->buffer, data.c_str(), data.size());
    sendOverlapped->wsaBuf.len = static_cast<ULONG>(data.size());

    // WSASend 호출
    DWORD bytesSent = 0;
    int result = WSASend(
        socket_,                    // 소켓
        &sendOverlapped->wsaBuf,    // 버퍼 정보
        1,                          // 버퍼 개수
        &bytesSent,                 // 전송된 바이트 수
        0,                          // 플래그
        sendOverlapped,             // OVERLAPPED 구조체
        nullptr                     // 완료 루틴
    );

    // 결과 처리 부분
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            // 실제 에러 발생 - 에러 코드를 자세히 로깅
            std::cerr << "WSASend failed: error=" << error << " (WSAGetLastError)" << std::endl;
            delete sendOverlapped;
            return false;
        }

        // WSA_IO_PENDING: 비동기 작업이 진행 중일 경우
    }

    return true;
}

void Session::ProcessRecv(size_t bytesTransferred, struct OverlappedEx* overlapped) {
    // 연결 종료 확인
    if (bytesTransferred == 0) {
        std::cout << "클라이언트가 연결을 종료했습니다 (소켓: " << socket_ << ")" << std::endl;
        Close();
        return;
    }

    // 받은 데이터를 문자열로 변환
    std::string receivedData;
    if (overlapped && overlapped->wsaBuf.buf) {
        // overlapped->buffer에 저장된 바이트를 string으로 복사
        receivedData.assign(overlapped->buffer, overlapped->buffer + bytesTransferred);
    } else {
        // 안전하게 빈 문자열 처리
        receivedData = std::string();
    }

    std::cout << "수신 데이터 (" << bytesTransferred << " bytes): " << receivedData << std::endl;
    
    // 상태별 패킷 처리 분배
    switch (currentState_) {
        case SessionState::AUTHENTICATING:
            // SessionManager에 위임
            if (auto sessionManager = GetSessionManager()) {
                sessionManager->HandleAuthProtocol(this, receivedData);
            }
            break;
            
        case SessionState::WAITING_MATCH:
        case SessionState::IN_LOBBY:
            // SessionManager에 직접 위임
            if (auto sessionManager = GetSessionManager()) {
                sessionManager->HandleLobbyPacket(this, receivedData);
            }
            break;
            
        case SessionState::IN_GAME:
            // GameManager에 위임
            if (auto gm = GetGameManager()) {
                // 실제 게임 로직이 구현된 GameManager가 있으면 전달
                gm->HandleGamePacket(this, receivedData);
            } else {
                // GameManager가 할당되지 않은 경우(예: 아직 매칭 미완료 등)
                std::cout << "[GAME] 패킷 처리: GameManager 미할당 - " << receivedData << std::endl;
                PostSend(PKT_GAME_NOT_IMPLEMENTED);
            }
            break;
            
        default:
            std::cerr << "Unknown session state" << std::endl;
            break;
    }
    
    // 다음 수신 준비
    if (!PostRecv()) {
        std::cerr << "다음 수신 요청 실패" << std::endl;
        Close();
    }
}

void Session::ProcessSend(size_t bytesTransferred) {
    std::cout << "데이터 송신 완료: " << bytesTransferred << " bytes (소켓: " << socket_ << ")" << std::endl;
}

// 전역 매니저들을 서버를 통해 접근
NetworkManager* Session::GetNetworkManager() const {
    return server_ ? server_->GetNetworkManager() : nullptr;
}

DatabaseManager* Session::GetDatabaseManager() const {
    return &DatabaseManager::GetInstance();
}

SessionManager* Session::GetSessionManager() const {
    return server_ ? server_->GetSessionManager() : nullptr;
}


