#include "IOCPServer.h"
#include "Session.h"
#include "DatabaseManager.h"
#include "NetworkManager.h"
#include "SessionManager.h"
#include "GameManager.h"
#include <ctime>

IOCPServer::IOCPServer(int port) 
    : port(port), isRunning(false)
{
}

IOCPServer::~IOCPServer() {
    Stop();
}

bool IOCPServer::Initialize() {
    // DatabaseManager 싱글톤 초기화
    if (!DatabaseManager::InitializeSingleton("db/user.db")) {
        std::cerr << "DatabaseManager 싱글톤 초기화 실패" << std::endl;
        return false;
    }

    sessionManager_ = std::make_unique<SessionManager>(this);

    networkManager_ = std::make_unique<NetworkManager>(port, this);
    if (!networkManager_->Initialize()) {
        std::cerr << "NetworkManager 초기화 실패" << std::endl;
        return false;
    }

    return true;
}

void IOCPServer::Start() {
    if (isRunning) return;
    isRunning = true;

    if (networkManager_) {
        networkManager_->StartAccept();
    }
}

void IOCPServer::Stop() {
    if (!isRunning) return;
    isRunning = false;   
    
    if (networkManager_) {
        networkManager_->Shutdown();
    }

    {
        std::lock_guard<std::mutex> lock(gamesMutex_);
        std::cout << "Stopping " << activeGames_.size() << " active games..." << std::endl;
        activeGames_.clear(); // unique_ptr들이 자동으로 GameManager 소멸
    }

    // 3. 모든 세션 종료
    if (sessionManager_) {
        sessionManager_->DisconnectAll();
    }
    
    std::cout << "IOCPServer stopped successfully." << std::endl;
}

// 세션 제거 요청을 SessionManager에 전달
void IOCPServer::RemoveSession(SOCKET socket) {
    if (sessionManager_) {
        sessionManager_->RemoveSession(socket);
    }
}

// 세션 추가 요청을 SessionManager에 전달  
bool IOCPServer::AddSession(std::shared_ptr<Session> session) {
    if (sessionManager_) {
        sessionManager_->AddSession(session);
    } else return false;
    
    // 세션에 Server 참조 설정 (전역 매니저들 접근용)
    session->SetServer(this);

    return true;
}

void IOCPServer::CreateGameRoom(const std::vector<std::shared_ptr<Session>>& players) {
    if (players.size() != GameManager::MAX_PLAYERS) {
        std::cerr << "Invalid player count for game room creation: " << players.size() << std::endl;
        return;
    }

    std::string roomId = "room_" + std::to_string(std::time(nullptr));
    auto gameManager = std::make_unique<GameManager>(roomId);

    try {
        std::cout << "게임 룸 생성 시작" << std::endl;

        // Insert game manager into active map
        {
            std::lock_guard<std::mutex> lock(gamesMutex_);
            activeGames_[roomId] = std::move(gameManager);
            std::cout << "[IOCPServer] activeGames_ inserted roomId=" << roomId << std::endl;
        }

        GameManager* gmPtr = nullptr;
        try {
            {
                std::lock_guard<std::mutex> lock(gamesMutex_);
                auto it = activeGames_.find(roomId);
                if (it == activeGames_.end()) throw std::runtime_error("Insert failed");
                gmPtr = it->second.get();
            }

            std::cout << "[IOCPServer] Adding players to GameManager: count=" << players.size() << std::endl;
            for (auto& session : players) {
                if (session && !session->IsClosed()) {
                    gmPtr->AddPlayer(session.get(), session->GetNickname(), session->GetToken());
                    session->SetGameManager(gmPtr);
                    session->SetState(SessionState::IN_GAME);
                    session->SetInMatchingQueue(false);

                    std::cout << "플레이어 게임 추가: " << session->GetNickname() << std::endl;
                }
            }

            // 게임 시작
            std::cout << "[IOCPServer] Calling StartGame for room=" << roomId << std::endl;
            bool started = false;
            try {
                started = gmPtr->StartGame();
            } catch (const std::exception& e) {
                std::cerr << "[IOCPServer] StartGame threw exception: " << e.what() << std::endl;
                throw; // rethrow to outer catch
            }

            if (started) {
                std::cout << "게임 시작 완료: " << roomId << std::endl;
            } else {
                std::cerr << "[IOCPServer] StartGame returned false for room=" << roomId << std::endl;
                throw std::runtime_error("StartGame returned false");
            }
        } catch (...) {
            // ensure we remove the partially-created game if any
            std::lock_guard<std::mutex> lock(gamesMutex_);
            auto it = activeGames_.find(roomId);
            if (it != activeGames_.end()) {
                activeGames_.erase(it);
                std::cerr << "[IOCPServer] Removed partial game room: " << roomId << std::endl;
            }
            throw; // allow outer handler to manage notification
        }
        
    } catch (const std::exception& e) {
        std::cerr << "게임 룸 생성 오류: " << e.what() << std::endl;
        
        // 롤백: activeGames_에 추가된 항목이 있다면 제거
        {
            std::lock_guard<std::mutex> lock(gamesMutex_);
            auto it = activeGames_.find(roomId);
            if (it != activeGames_.end()) {
                activeGames_.erase(it);
            }
        }

        // 오류 발생 시 플레이어들을 로비로 되돌림
        for (auto& session : players) {
            if (session && !session->IsClosed()) {
                session->SetState(SessionState::IN_LOBBY);
                session->SetGameManager(nullptr);
                session->SetInMatchingQueue(false);
                session->PostSend("GAME_CREATE_ERROR");
            }
        }
    }
}

void IOCPServer::RemoveGameRoom(const std::string& roomId) {
    std::lock_guard<std::mutex> lock(gamesMutex_);
    auto it = activeGames_.find(roomId);
    if (it != activeGames_.end()) {
        std::cout << "Removing game room: " << roomId << std::endl;
        activeGames_.erase(it);
    }
}