#include "../../include/core/PacketHandler.h"
#include "../../include/core/PacketProtocol.h"
#include "../../include/core/GameState.h"
#include "../../include/gui/ConsoleUtils.h"
#include "../../include/core/Logger.h"
#include <iostream>
#include <sstream>
#include <algorithm>

PacketHandler::PacketHandler(std::shared_ptr<GameState> gameState)
    : gameState_(gameState) {
    // 기본 핸들러들 등록
    RegisterDefaultHandlers();
}

void PacketHandler::RegisterHandler(const std::string& packetType, PacketCallback callback) {
    handlers_[packetType] = callback;
}

void PacketHandler::RegisterDefaultHandlers() {
    // 인증 패킷 (SSL)
    RegisterHandler(PKT_SIGNUP_OK, [this](const std::string& data) { HandleSignupOk(data); });
    RegisterHandler(PKT_SIGNUP_ERROR, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_LOGIN_OK, [this](const std::string& data) { HandleLoginOk(data); });
    RegisterHandler(PKT_LOGIN_NO_ACCOUNT, [this](const std::string& data) { HandleLoginFailure("Account not found"); });
    RegisterHandler(PKT_LOGIN_WRONG_PW, [this](const std::string& data) { HandleLoginFailure("Wrong password"); });
    RegisterHandler(PKT_LOGIN_SUSPENDED, [this](const std::string& data) { HandleLoginFailure("Account suspended"); });
    RegisterHandler(PKT_LOGIN_ERROR, [this](const std::string& data) { HandleLoginFailure("Login error"); });
    RegisterHandler(PKT_NICKNAME_EDIT_OK, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_SIGNUP_DUPLICATE, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_NICKNAME_EDIT_ERROR, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_CHECK_ID_DUPLICATE, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_CHECK_ID_OK, [this](const std::string& data) { HandleError(data); });

    // 사용자 정보 패킷 (TCP)
    // server sends TOKEN_VALID|nickname for validated tokens / profile info
    RegisterHandler(PKT_TOKEN_VALID, [this](const std::string& data) { HandleUserProfile(data); });
    RegisterHandler(PKT_INVALID_TOKEN, [this](const std::string& data) { HandleInvalidToken(data); });
    // Authentication errors (server may send AUTH_ERROR|reason)
    RegisterHandler(PKT_AUTH_ERROR, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_SIGNUP_DUPLICATE, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_NICKNAME_EDIT_OK, [this](const std::string& data) { HandleError(data); });

    // 게임 프로토콜 패킷
    RegisterHandler(PKT_WAIT_REPLY, [this](const std::string& data) { HandleWaitReply(data); });
    RegisterHandler(PKT_QUEUE_FULL, [this](const std::string& data) { HandleQueueFull(data); });
    RegisterHandler(PKT_SESSION_ACK, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_SESSION_NOT_FOUND, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_GAME_INIT, [this](const std::string& data) { HandleGameInit(data); });
    RegisterHandler(PKT_GAME_START, [this](const std::string& data) { HandleGameStart(data); });
    RegisterHandler(PKT_ALL_CARDS, [this](const std::string& data) { HandleAllCards(data); });
    RegisterHandler(PKT_ROLE_INFO, [this](const std::string& data) { HandleRoleInfo(data); });
    RegisterHandler(PKT_TURN_UPDATE, [this](const std::string& data) { HandleTurnUpdate(data); });
    RegisterHandler(PKT_HINT_MSG, [this](const std::string& data) { HandleHintMsg(data); });
    RegisterHandler(PKT_CARD_UPDATE, [this](const std::string& data) { HandleCardUpdate(data); });
    RegisterHandler(PKT_CHAT_MSG, [this](const std::string& data) { HandleChatMsg(data); });
    RegisterHandler(PKT_GAME_OVER, [this](const std::string& data) { HandleGameOver(data); });

    // 기타
    RegisterHandler(PKT_ERROR, [this](const std::string& data) { HandleError(data); });
}

void PacketHandler::ProcessPacket(const std::string& packet) {
    if (packet.empty()) {
        std::cerr << "Empty packet received" << std::endl;
        return;
    }

    size_t delimiterPos = packet.find('|');
    
    std::string packetType;
    if (delimiterPos == std::string::npos) {
        // 데이터가 없는 패킷 (예: "ERROR", "INVALID_TOKEN")
        packetType = packet;
    } else {
        packetType = packet.substr(0, delimiterPos);
    }

    // 해당 패킷 타입의 핸들러 찾기
    auto it = handlers_.find(packetType);
    
    if (it == handlers_.end()) {
        std::cerr << "No handler registered for packet type: " << packetType << std::endl;
        return;
    }

    // 데이터 추출 (파이프 이후)
    std::string packetData;
    if (delimiterPos != std::string::npos) {
        packetData = packet.substr(delimiterPos + 1);
    }

    // 등록된 핸들러 호출
    try {
        it->second(packetData);
    } catch (const std::exception& e) {
        std::cerr << "Error processing packet " << packetType << ": " << e.what() << std::endl;
    }
}

std::string PacketHandler::ParseField(const std::string& data, int fieldIndex, const std::string& delimiter) {
    size_t startPos = 0;
    int currentField = 0;

    while (currentField < fieldIndex && startPos < data.size()) {
        startPos = data.find(delimiter, startPos) + delimiter.size();
        currentField++;
    }

    if (currentField != fieldIndex) {
        return "";  // 필드를 찾지 못함
    }

    size_t endPos = data.find(delimiter, startPos);
    if (endPos == std::string::npos) {
        return data.substr(startPos);  // 마지막 필드
    }

    return data.substr(startPos, endPos - startPos);
}

// ==================== 인증 패킷 핸들러 ====================

void PacketHandler::HandleSignupOk(const std::string& data) {
    // SIGNUP_OK|token
    std::string token = ParseField(data, 0, "|");
    if (!token.empty()) {
        gameState_->token = token;
        gameState_->SetPhase(GamePhase::LOBBY);
        Logger::Info(std::string("Signup successful. Token: ") + token);
    }
}

void PacketHandler::HandleLoginOk(const std::string& data) {
    // LOGIN_OK|token
    std::string token = ParseField(data, 0, "|");
    if (!token.empty()) {
        gameState_->token = token;
        gameState_->SetPhase(GamePhase::LOBBY);
        Logger::Info(std::string("Login successful. Token: ") + token);
    }
}

void PacketHandler::HandleInvalidToken(const std::string& data) {
    // INVALID_TOKEN
    gameState_->SetPhase(GamePhase::ERROR_PHASE);
    Logger::Warn("Invalid token. Please login again.");
}

// ==================== 사용자 정보 패킷 핸들러 ====================

void PacketHandler::HandleUserProfile(const std::string& data) {
    // TOKEN_VALID|nickname  (server currently sends TOKEN_VALID with nickname)
    std::string nickname = ParseField(data, 0, "|");

    if (!nickname.empty()) {
        gameState_->username = nickname;
        Logger::Info(std::string("User profile: ") + nickname);
    }
}

// ==================== 게임 프로토콜 핸들러 ====================

void PacketHandler::HandleWaitReply(const std::string& data) {
    // WAIT_REPLY|playerCount
    // spec: WAIT_REPLY|playerCount|maxPlayers
    std::string countStr = ParseField(data, 0, "|");
    std::string maxStr = ParseField(data, 1, "|");
    if (!countStr.empty()) {
        int count = std::stoi(countStr);
        gameState_->matchingCount = count;
        if (!maxStr.empty()) {
            gameState_->matchingMax = std::stoi(maxStr);
        }
        // 저장: GUI가 이를 읽어 진행 바를 그리도록 함
        gameState_->SetPhase(GamePhase::MATCHING);
        Logger::Info(std::string("Matching in progress: ") + std::to_string(count) +
                    (gameState_->matchingMax > 0 ? (std::string(" / ") + std::to_string(gameState_->matchingMax)) : std::string("")));
    }
}

void PacketHandler::HandleQueueFull(const std::string& data) {
    // QUEUE_FULL
    gameState_->SetPhase(GamePhase::MATCHING);
    Logger::Info("Queue full! Game starting...");
}

void PacketHandler::HandleGameInit(const std::string& data) {
    // GAME_INIT|nick1|role1|team1|leader1|nick2|role2|team2|leader2|...
    Logger::Info(std::string("HandleGameInit received: ") + data);
    
    std::vector<Player> players;
    
    // 자신의 닉네임을 가져옴 (GameState에 저장된 username 사용)
    std::string myNickname = gameState_->username;
    Logger::Info(std::string("Looking for my nickname: '") + myNickname + "'");
    int myIndex = -1;
    
    for (int i = 0; i < 6; ++i) {  // Original C 버전은 6명까지 지원
        int baseField = i * 4;
        std::string nick = ParseField(data, baseField, "|");
        std::string roleStr = ParseField(data, baseField + 1, "|");
        std::string teamStr = ParseField(data, baseField + 2, "|");
        std::string leaderStr = ParseField(data, baseField + 3, "|");
        
        if (!nick.empty()) {
            Player p;
            p.nickname = nick;
            p.role = (std::stoi(roleStr) % 2 == 0) ? PlayerRole::SPYMASTER : PlayerRole::AGENT;
            p.team = std::stoi(teamStr);
            p.isLeader = (std::stoi(leaderStr) == 1);
            p.isReady = false;
            players.push_back(p);
            
            Logger::Info(std::string("Player ") + std::to_string(i) + ": '" + nick + 
                        "', team=" + teamStr + ", leader=" + leaderStr);
            
            // 자신의 닉네임과 매칭되면 myPlayerIndex 설정
            if (nick == myNickname) {
                myIndex = i;
                Logger::Info(std::string("*** MATCH FOUND at index ") + std::to_string(i) + " ***");
            }
        }
    }
    
    // myPlayerIndex와 관련 정보 설정
    if (myIndex >= 0 && myIndex < static_cast<int>(players.size())) {
        gameState_->myPlayerIndex = myIndex;
        gameState_->myTeam = players[myIndex].team;
        gameState_->isMyLeader = players[myIndex].isLeader;
        
        Logger::Info(std::string("==> Game initialized - My index: ") + std::to_string(myIndex) + 
                     ", Team: " + std::to_string(gameState_->myTeam) + 
                     ", Leader: " + (gameState_->isMyLeader ? "Yes" : "No"));
    } else {
        Logger::Warn(std::string("!!! Could not find my player index for nickname: '") + myNickname + "' !!!");
    }
    
    gameState_->UpdatePlayers(players);
    Logger::Info(std::string("Game initialized with ") + std::to_string(players.size()) + " players");
}

void PacketHandler::HandleGameStart(const std::string& data) {
    // GAME_START|sessionId
    std::string sessionIdStr = ParseField(data, 0, "|");
    if (!sessionIdStr.empty()) {
        gameState_->sessionId = std::stoi(sessionIdStr);
        gameState_->SetPhase(GamePhase::PLAYING);
        Logger::Info(std::string("Game started. Session ID: ") + std::to_string(gameState_->sessionId));
    }
}

void PacketHandler::HandleAllCards(const std::string& data) {
    // ALL_CARDS|word1|type1|isUsed1|word2|type2|isUsed2|...
    Logger::Info(std::string("ALL_CARDS received - parsing..."));
    
    std::vector<GameCard> cards;
    
    for (int i = 0; i < 25; ++i) {
        int baseField = i * 3;
        std::string word = ParseField(data, baseField, "|");
        std::string typeStr = ParseField(data, baseField + 1, "|");
        std::string usedStr = ParseField(data, baseField + 2, "|");
        
        if (!word.empty()) {
            GameCard c;
            c.word = word;
            c.cardType = std::stoi(typeStr);
            c.isRevealed = (std::stoi(usedStr) == 1);
            cards.push_back(c);
            
            // 디버깅: revealed 카드 로그
            if (c.isRevealed) {
                Logger::Info(std::string("Card[") + std::to_string(i) + 
                            "] is already revealed: " + c.word + 
                            " (type=" + std::to_string(c.cardType) + ")");
            }
        }
    }

    gameState_->UpdateCards(cards);
    Logger::Info(std::string("Received ") + std::to_string(cards.size()) + " cards");
}

void PacketHandler::HandleRoleInfo(const std::string& data) {
    // ROLE_INFO|roleNumber (0-3)
    // 이 패킷은 GAME_INIT 이후에 오므로, 이미 설정된 정보와 일치하는지 검증
    std::string roleStr = ParseField(data, 0, "|");
    if (!roleStr.empty()) {
        int role = std::stoi(roleStr);
        gameState_->myRole = role;
        
        // GAME_INIT에서 이미 설정되었지만, 백업으로 다시 설정
        // (만약 GAME_INIT에서 myPlayerIndex를 찾지 못한 경우를 대비)
        if (gameState_->myPlayerIndex == -1) {
            gameState_->isMyLeader = (role == 0 || role == 2);  // 0, 2는 리더
            gameState_->myTeam = (role < 2) ? 0 : 1;  // 0,1은 팀0, 2,3은 팀1
            Logger::Warn(std::string("Using ROLE_INFO as fallback - Role: ") + std::to_string(role));
        } else {
            Logger::Info(std::string("ROLE_INFO received - Role: ") + std::to_string(role) + 
                        " (already set from GAME_INIT)");
        }
    }
}

void PacketHandler::HandleTurnUpdate(const std::string& data) {
    // TURN_UPDATE|team|phase|redScore|blueScore
    std::string teamStr = ParseField(data, 0, "|");
    std::string phaseStr = ParseField(data, 1, "|");
    std::string redStr = ParseField(data, 2, "|");
    std::string blueStr = ParseField(data, 3, "|");
    
    if (!teamStr.empty() && !redStr.empty() && !blueStr.empty()) {
        gameState_->inGameStep = std::stoi(phaseStr);
        gameState_->SetTurn(std::stoi(teamStr));
        gameState_->UpdateScore(std::stoi(redStr), std::stoi(blueStr));
    }
}

void PacketHandler::HandleHintMsg(const std::string& data) {
    // HINT|team|word|count
    std::string teamStr = ParseField(data, 0, "|");
    std::string word = ParseField(data, 1, "|");
    std::string countStr = ParseField(data, 2, "|");
    
    if (!word.empty() && !countStr.empty()) {
        int count = std::stoi(countStr);
        gameState_->SetHint(word, count);
        gameState_->remainingTries = count;  // 남은 시도 횟수 초기화
        Logger::Info(std::string("Hint received: ") + word + " (" + countStr + ")");
    }
}

void PacketHandler::HandleCardUpdate(const std::string& data) {
    // CARD_UPDATE|cardIndex|isUsed|remainingTries
    std::string indexStr = ParseField(data, 0, "|");
    std::string usedStr = ParseField(data, 1, "|");
    std::string triesStr = ParseField(data, 2, "|");
    
    Logger::Info(std::string("CARD_UPDATE received: ") + data);
    
    if (!indexStr.empty()) {
        int index = std::stoi(indexStr);
        int isUsed = usedStr.empty() ? 0 : std::stoi(usedStr);
        
        gameState_->RevealCard(index);
        
        // remainingTries 업데이트 (서버에서 전송)
        if (!triesStr.empty()) {
            gameState_->remainingTries = std::stoi(triesStr);
            Logger::Info(std::string("Card revealed: ") + std::to_string(index) + 
                        ", Remaining tries: " + std::to_string(gameState_->remainingTries));
        } else {
            Logger::Info(std::string("Card revealed: ") + std::to_string(index));
        }
    }
}

void PacketHandler::HandleChatMsg(const std::string& data) {
    // CHAT|team|roleNum|nickname|message
    std::string teamStr = ParseField(data, 0, "|");
    std::string roleStr = ParseField(data, 1, "|");
    std::string nickname = ParseField(data, 2, "|");
    std::string message = ParseField(data, 3, "|");
    
    if (!nickname.empty() && !message.empty()) {
        GameMessage msg;
        msg.nickname = nickname;
        msg.message = message;
        msg.team = teamStr.empty() ? 999 : std::stoi(teamStr);
        
        gameState_->AddMessage(msg);
        Logger::Info(std::string("[") + nickname + "]: " + message);
    }
}

void PacketHandler::HandleGameOver(const std::string& data) {
    // GAME_OVER|winnerTeam (0: RED, 1: BLUE, -1: DRAW)
    std::string winnerStr = ParseField(data, 0, "|");
    
    if (!winnerStr.empty()) {
        int winner = std::stoi(winnerStr);
        std::string winnerName = (winner == 0) ? "RED" : 
                                (winner == 1) ? "BLUE" : "DRAW";
        
        Logger::Info(std::string("GAME_OVER - Winner: ") + winnerName);
        
        // 게임 오버 메시지 추가
        GameMessage msg;
        msg.nickname = "SYSTEM";
        msg.message = winnerName + " 팀이 승리했습니다! (ESC: 로비로)";
        msg.team = 2;  // SYSTEM
        gameState_->AddMessage(msg);
        
        // 게임 오버 콜백 호출
        gameState_->OnGameOver();
    }
}

// ==================== 기타 핸들러 ====================

void PacketHandler::HandleError(const std::string& data) {
    // ERROR
    gameState_->SetPhase(GamePhase::ERROR_PHASE);
    Logger::Error("Server error occurred");
    // Show generic error status on GUI border
    ConsoleUtils::SetStatus("Server error");
    // leave additional behavior to specific handlers
}

void PacketHandler::HandleLoginFailure(const std::string& reason) {
    // Don't move to ERROR_PHASE for ordinary login failures; instead show a status
    Logger::Warn(std::string("Login failed: ") + reason);
    ConsoleUtils::SetStatus(std::string("Login failed: ") + reason);
}
