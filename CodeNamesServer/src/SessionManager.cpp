#include "SessionManager.h"
#include "Session.h"
#include "DatabaseManager.h"
#include "GameManager.h"
#include <iostream>
#include <vector>
#include <thread>
#include <ctime>
#include "IOCPServer.h"

SessionManager::SessionManager(IOCPServer* server) : server_(server), sessionCount_(0) {
    if (!server_) {
        throw std::runtime_error("SessionManager: IOCPServer pointer cannot be null");
    }
}

SessionManager::~SessionManager() {
    DisconnectAll();
}

bool SessionManager::AddSession(std::shared_ptr<Session> session) {
    if (!session) return false;

    std::lock_guard<std::mutex> lock(sessionMutex_);
    SOCKET socket = session->GetSocket();
    if (sessions_.count(socket) > 0) {
        std::cerr << "AddSession 실패: 중복 소켓 " << socket << std::endl;
        return false; // 중복 소켓
    }

    sessions_[socket] = session;
    const std::string& token = session->GetToken();
    if (!token.empty()) {
        if (tokenToSocket_.count(token) > 0) {
            std::cerr << "AddSession 실패: 중복 토큰 " << token << std::endl;
            sessions_.erase(socket); // 롤백
            return false; // 중복 토큰
        }
        tokenToSocket_[token] = socket;
    }

    ++sessionCount_;
    std::cout << "Session added: " << socket << " (Total: " << sessionCount_ << ")" << std::endl;
    return true;
}

void SessionManager::RemoveSession(SOCKET socket) {
    if (socket == INVALID_SOCKET) return;

    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto target = sessions_.find(socket);

    if (target != sessions_.end()) {
        const std::string& token = target->second->GetToken();
        if (!token.empty()) {
            tokenToSocket_.erase(token);
        }
        sessions_.erase(target);
        --sessionCount_;

        std::cout << "Session removed: " << socket << " (Total: " << sessionCount_ << ")" << std::endl;
    }
}

std::shared_ptr<Session> SessionManager::FindSession(SOCKET socket) {
    std::lock_guard<std::mutex> lock(sessionMutex_);

    auto target = sessions_.find(socket);
    if (target != sessions_.end()) {
        return target->second;
    }

    return nullptr;
}

std::shared_ptr<Session> SessionManager::FindSessionByToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex_);

    auto target = tokenToSocket_.find(token);
    if (target != tokenToSocket_.end()) {
        SOCKET socket = target->second;
        return FindSession(socket);
    }
    
    return nullptr;
}

bool SessionManager::ValidateToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return tokenToSocket_.count(token) == 0; // 중복 없음
}

bool SessionManager::AddToMatchingQueue(std::shared_ptr<Session> session) {
    if (!session) return false;

    std::lock_guard<std::mutex> lock(sessionMutex_);
    SOCKET socket = session->GetSocket();

    // 세션 존재 확인
    if (sessions_.count(socket) == 0) {
        std::cerr << "Session not found for matching queue: " << socket << std::endl;
        return false;
    }

    matchingQueue_.push(socket);
    session->SetInMatchingQueue(true);
    std::cout << "Session added to matching queue: " << socket << std::endl;
    return true;
}

void SessionManager::RemoveFromMatchingQueue(std::shared_ptr<Session> session) {
    if (!session) return;

    std::lock_guard<std::mutex> lock(sessionMutex_);
    // 큐에서 실제 제거하지 않고 상태만 변경
    session->SetInMatchingQueue(false);
    // 큐는 그대로 두고, GetWaitingPlayers에서 필터링
}

void SessionManager::RequestGameRoomCreation(const std::vector<std::shared_ptr<Session>>& players) {
    if (players.size() != GameManager::MAX_PLAYERS) {
        std::cerr << "Invalid player count for game room creation: " << players.size() << std::endl;
        return;
    }

    std::cout << "Requesting game room creation through IOCPServer..." << std::endl;
    server_->CreateGameRoom(players);
}

std::vector<std::shared_ptr<Session>> SessionManager::GetWaitingPlayers() {
    std::vector<std::shared_ptr<Session>> waitingPlayers;
    
    std::lock_guard<std::mutex> lock(sessionMutex_);
    // 매칭 큐에서 유효한 세션들만 수집
    std::queue<SOCKET> cleanQueue;
    // 지연 처리 방식으로 효율적이라고 한다.
    while (!matchingQueue_.empty()) {
        SOCKET socket = matchingQueue_.front();
        matchingQueue_.pop();

        auto sessionIt = sessions_.find(socket);
        if (sessionIt != sessions_.end() && sessionIt->second->IsInMatchingQueue()) {
            waitingPlayers.push_back(sessionIt->second);
            cleanQueue.push(socket); // 유효한 세션만 다시 큐에 추가
        }
        // 무효한 세션은 큐에서 제거
    }

    // 정리된 큐로 교체
    matchingQueue_ = std::move(cleanQueue);

    return waitingPlayers;
}

void SessionManager::BroadcastToAll(const std::string& message) {
    if (message.empty()) return;

    // 락을 최소화 하기 위해 세션 목록 복사
    std::vector<std::shared_ptr<Session>> sessionList;
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        sessionList.reserve(sessions_.size());

        for (const auto& pair : sessions_) {
            sessionList.push_back(pair.second);
        }
    }

    // 락 해제 후 브로드캐스트
    for (const auto& session : sessionList) {
        if (session && !session->IsClosed()) {
            session->PostSend(message);
        }
    }

    std::cout << "Broadcasted to " << sessionList.size() << " sessions: " << message << std::endl;
}

size_t SessionManager::GetSessionCount() const {
    return sessionCount_.load();
}

void SessionManager::DisconnectAll() {
    std::vector<std::shared_ptr<Session>> sessionList;
    
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        sessionList.reserve(sessions_.size());
        
        for (const auto& pair : sessions_) {
            sessionList.push_back(pair.second);
        }
        
        // 컨테이너 정리
        sessions_.clear();
        tokenToSocket_.clear();
        std::queue<SOCKET> empty;
        matchingQueue_.swap(empty);
        sessionCount_ = 0;
    }

    // 락 해제 후 세션 종료
    for (const auto& session : sessionList) {
        if (session && !session->IsClosed()) {
            session->Close();
        }
    }

    std::cout << "All sessions disconnected." << std::endl;
}

// 게임 룸 제거는 IOCPServer에서 처리

// 로비/매칭 패킷 처리
void SessionManager::HandleLobbyPacket(Session* session, const std::string& data) {
    std::cout << "[SessionManager] 로비 패킷 처리: " << data << std::endl;
    
    if (data.find("CMD|QUERY_WAIT|") == 0) // CMD|QUERY_WAIT|{token} - 매칭 대기 요청
    {
        std::string token = data.substr(15);

        if (token == session->GetToken()) 
        { // 매칭 큐에 추가
            if (AddToMatchingQueue(session->shared_from_this())) {
                // 현재 대기 인원 확인
                auto waitingPlayers = GetWaitingPlayers();

                if (waitingPlayers.size() == GameManager::MAX_PLAYERS) {
                    // 게임 생성 처리
                    std::thread gameThread(&SessionManager::RequestGameRoomCreation, this, waitingPlayers);
                    gameThread.detach();  // 스레드를 detach하여 독립적으로 실행

                    for (auto& player : waitingPlayers) {
                        player->PostSend("QUEUE_FULL"); 
                    }
                } else {
                    std::string waitMsg = "WAIT_REPLY|" + std::to_string(waitingPlayers.size()) + "|" + std::to_string(GameManager::MAX_PLAYERS);
                
                    // 대기자에게 현재 상황 전송
                    for (auto& player : waitingPlayers) {
                        player->PostSend(waitMsg);
                    }
                }
            } else {
                session->PostSend("QUEUE_ERROR");
            }
        } else {
            session->PostSend("INVALID_TOKEN");
        }
    } else if (data.find("SESSION_READY|") == 0) {
        std::string token = data.substr(14);

        if(token == session->GetToken()) {
            session->PostSend("SESSION_ACK");
        } else {
            session->PostSend("SESSION_NOT_FOUND");
        }
    } else if (data.find("MATCHING_CANCEL|") == 0) {
        // MATCHING_CANCEL|{token} - 매칭 취소
        std::string token = data.substr(16);
        if (token == session->GetToken()) {
            RemoveFromMatchingQueue(session->shared_from_this());
            session->PostSend("CANCEL_OK");
        } else {
            session->PostSend("CANCEL_OK");
        }
    }
    else {
        std::cerr << "Unknown lobby packet: " << data << std::endl;
        session->PostSend("LOBBY_ERROR|UNKNOWN_PACKET");
    }
}

// 인증 관련 프로토콜 처리 (CHECK_ID, SIGNUP, LOGIN, TOKEN, EDIT_NICK)
void SessionManager::HandleAuthProtocol(Session* session, const std::string& data) {
    std::cout << "[SessionManager] 인증 패킷 처리: " << data << std::endl;
    
    if(data.find("CHECK_ID|") == 0) 
    {
        // CHECK_ID|{id} - ID 중복 검사
        std::string id = data.substr(9);

        if (auto dbManager = session->GetDatabaseManager()) {
            if (dbManager->CheckIdExists(id)) {
                session->PostSend("CHECK_ID_DUPLICATE");
            } else {
                session->PostSend("CHECK_ID_OK");
            }
        } else {
            session->PostSend("CHECK_ID_ERROR");
        }
        
    } else if (data.find("SIGNUP|") == 0)
    {
        // SIOGNUP|{id}|{password}|{nickname} - 회원가입
        size_t first_pipe = data.find('|', 7);
        size_t second_pipe = data.find('|', first_pipe + 1);

        if (first_pipe != std::string::npos && second_pipe != std::string::npos)
        {
            std::string id = data.substr(7, first_pipe - 7);
            std::string pw = data.substr(first_pipe + 1, second_pipe - first_pipe - 1);
            std::string nick = data.substr(second_pipe + 1);

            if (auto dbManager = session->GetDatabaseManager()) {
                DatabaseResult result = dbManager->SignupUser(id, pw, nick);
                if (result == DatabaseResult::SUCCESS) {
                    std::string token = dbManager->GenerateToken();
                    session->SetToken(token);
                    session->SetUserName(nick);
                    session->PostSend("SIGNUP_OK|" + token);
                } else if (result == DatabaseResult::NICK_DUPLICATE) {
                    session->PostSend("SIGNUP_DUPLICATE");
                } else {
                    session->PostSend("SIGNUP_ERROR");
                }
            } else {
                session->PostSend("SIGNUP_ERROR");
            }
        } else {
            session->PostSend("SIGNUP_ERROR");
        }
    } else if (data.find("LOGIN|") == 0) {
        // LOGIN|{id}|{pw} - 로그인
        size_t pipe_pos = data.find('|', 6);

        if (pipe_pos != std::string::npos) {
            std::string id = data.substr(6, pipe_pos - 6);
            std::string pw = data.substr(pipe_pos + 1);
            if (auto dbManager = session->GetDatabaseManager()) {
                DatabaseResult result = dbManager->LoginUser(id, pw);
                if (result == DatabaseResult::SUCCESS) {
                    // 사용자 정보 로드
                    auto userInfo = dbManager->GetUserInfoByToken(id);
                    if (userInfo) {
                        session->SetUserInfo(*userInfo);  // Session에 사용자 정보 저장
                        session->SetLoggedIn(true);
                        
                        std::string token = dbManager->GenerateToken();
                        session->SetToken(token);
                        session->SetUserName(userInfo->nickname);
                        session->PostSend("LOGIN_OK|" + token);
                        session->SetState(SessionState::IN_LOBBY);
                    } else {
                        session->PostSend("LOGIN_ERROR");
                    }
                } else if (result == DatabaseResult::NOT_FOUND) {
                    session->PostSend("LOGIN_NO_ACCOUNT");
                } else if (result == DatabaseResult::WRONG_PASSWORD) {
                    session->PostSend("LOGIN_WRONG_PW");
                } else if (result == DatabaseResult::SUSPENDED) {
                    session->PostSend("LOGIN_SUSPENDED");
                } else {
                    session->PostSend("LOGIN_ERROR");
                }
            } else {
                session->PostSend("LOGIN_ERROR");
            }
        } else {
            session->PostSend("LOGIN_ERROR");
        } 
    } else if (data.find("TOKEN|") == 0) 
    {
        std::string token = data.substr(6);

        if (token == session->GetToken()) {
            session->PostSend("TOKEN_VALID|" + session->GetUserName());
        } else {
            session->PostSend("INVALID_TOKEN");
        }
    } else if (data.find("EDIT_NICK|") == 0)
    {
        // EDIT_NICK|{token}|{new_nick} - 닉네임 수정
        size_t pipe_pos = data.find('|', 10);
        
        if (pipe_pos != std::string::npos) {
            std::string token = data.substr(10, pipe_pos - 10);
            std::string new_nick = data.substr(pipe_pos + 1);
            
            if (token == session->GetToken()) {
                session->SetUserName(new_nick);
                session->PostSend("NICKNAME_EDIT_OK");
            } else {
                session->PostSend("INVALID_TOKEN");
            }
        } else {
            session->PostSend("NICKNAME_EDIT_ERROR");
        }
    } else 
    {
        std::cerr << "Unknown auth packet: " << data << std::endl;
        session->PostSend("AUTH_ERROR|UNKNOWN_PACKET");
    }
}