#include "../../include/core/PacketHandler.h"
#include "../../include/core/PacketProtocol.h"
#include "../../include/core/GameState.h"
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
    RegisterHandler(PKT_LOGIN_NO_ACCOUNT, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_LOGIN_WRONG_PW, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_LOGIN_SUSPENDED, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_LOGIN_ERROR, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_NICKNAME_EDIT_OK, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_NICK_DUPLICATE, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_NICKNAME_EDIT_ERROR, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_ID_TAKEN, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_ID_AVAILABLE, [this](const std::string& data) { HandleError(data); });

    // 사용자 정보 패킷 (TCP)
    RegisterHandler(PKT_NICKNAME, [this](const std::string& data) { HandleUserProfile(data); });
    RegisterHandler(PKT_INVALID_TOKEN, [this](const std::string& data) { HandleInvalidToken(data); });
    RegisterHandler(PKT_NICKNAME_TAKEN, [this](const std::string& data) { HandleError(data); });
    RegisterHandler(PKT_NICKNAME_AVAILABLE, [this](const std::string& data) { HandleError(data); });

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
        gameState_->currentPhase = GamePhase::LOBBY;
        std::cout << "Signup successful. Token: " << token << std::endl;
    }
}

void PacketHandler::HandleLoginOk(const std::string& data) {
    // LOGIN_OK|token
    std::string token = ParseField(data, 0, "|");
    if (!token.empty()) {
        gameState_->token = token;
        gameState_->currentPhase = GamePhase::LOBBY;
        std::cout << "Login successful. Token: " << token << std::endl;
    }
}

void PacketHandler::HandleInvalidToken(const std::string& data) {
    // INVALID_TOKEN
    gameState_->currentPhase = GamePhase::ERROR_PHASE;
    std::cout << "Invalid token. Please login again." << std::endl;
}

// ==================== 사용자 정보 패킷 핸들러 ====================

void PacketHandler::HandleUserProfile(const std::string& data) {
    // NICKNAME|nickname|WINS|wins|LOSSES|losses
    std::string nickname = ParseField(data, 0, "|");
    std::string winsStr = ParseField(data, 2, "|");
    std::string lossesStr = ParseField(data, 4, "|");

    if (!nickname.empty() && !winsStr.empty() && !lossesStr.empty()) {
        gameState_->username = nickname;
        std::cout << "User profile: " << nickname 
                 << " | Wins: " << winsStr 
                 << " | Losses: " << lossesStr << std::endl;
    }
}

// ==================== 게임 프로토콜 핸들러 ====================

void PacketHandler::HandleWaitReply(const std::string& data) {
    // WAIT_REPLY|playerCount
    std::string countStr = ParseField(data, 0, "|");
    if (!countStr.empty()) {
        int count = std::stoi(countStr);
        gameState_->SetPhase(GamePhase::MATCHING);
        std::cout << "Matching in progress: " << count << " players waiting" << std::endl;
    }
}

void PacketHandler::HandleQueueFull(const std::string& data) {
    // QUEUE_FULL
    gameState_->SetPhase(GamePhase::MATCHING);
    std::cout << "Queue full! Game starting..." << std::endl;
}

void PacketHandler::HandleGameInit(const std::string& data) {
    // GAME_INIT|nick1|role1|team1|leader1|nick2|role2|team2|leader2|...
    std::vector<Player> players;
    
    for (int i = 0; i < 4; ++i) {
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
        }
    }
    
    gameState_->UpdatePlayers(players);
    std::cout << "Game initialized with " << players.size() << " players" << std::endl;
}

void PacketHandler::HandleGameStart(const std::string& data) {
    // GAME_START|sessionId
    std::string sessionIdStr = ParseField(data, 0, "|");
    if (!sessionIdStr.empty()) {
        gameState_->sessionId = std::stoi(sessionIdStr);
        gameState_->SetPhase(GamePhase::PLAYING);
        std::cout << "Game started! Session ID: " << gameState_->sessionId << std::endl;
    }
}

void PacketHandler::HandleAllCards(const std::string& data) {
    // ALL_CARDS|word1|type1|isUsed1|word2|type2|isUsed2|...
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
        }
    }
    
    gameState_->UpdateCards(cards);
    std::cout << "Received " << cards.size() << " cards" << std::endl;
}

void PacketHandler::HandleRoleInfo(const std::string& data) {
    // ROLE_INFO|roleNumber (0-3)
    std::string roleStr = ParseField(data, 0, "|");
    if (!roleStr.empty()) {
        int role = std::stoi(roleStr);
        gameState_->myRole = role;
        gameState_->isMyLeader = (role == 0 || role == 2);  // 0, 2는 리더
        gameState_->myTeam = (role < 2) ? 0 : 1;  // 0,1은 팀0, 2,3은 팀1
        std::cout << "My role: " << role << std::endl;
    }
}

void PacketHandler::HandleTurnUpdate(const std::string& data) {
    // TURN_UPDATE|team|phase|redScore|blueScore
    std::string teamStr = ParseField(data, 0, "|");
    std::string phaseStr = ParseField(data, 1, "|");
    std::string redStr = ParseField(data, 2, "|");
    std::string blueStr = ParseField(data, 3, "|");
    
    if (!teamStr.empty() && !redStr.empty() && !blueStr.empty()) {
        gameState_->SetTurn(std::stoi(teamStr));
        gameState_->currentPhase_ = std::stoi(phaseStr);
        gameState_->UpdateScore(std::stoi(redStr), std::stoi(blueStr));
    }
}

void PacketHandler::HandleHintMsg(const std::string& data) {
    // HINT|team|word|count
    std::string teamStr = ParseField(data, 0, "|");
    std::string word = ParseField(data, 1, "|");
    std::string countStr = ParseField(data, 2, "|");
    
    if (!word.empty() && !countStr.empty()) {
        gameState_->SetHint(word, std::stoi(countStr));
        std::cout << "Hint received: " << word << " (" << countStr << ")" << std::endl;
    }
}

void PacketHandler::HandleCardUpdate(const std::string& data) {
    // CARD_UPDATE|cardIndex|isUsed
    std::string indexStr = ParseField(data, 0, "|");
    std::string usedStr = ParseField(data, 1, "|");
    
    if (!indexStr.empty()) {
        int index = std::stoi(indexStr);
        gameState_->RevealCard(index);
        std::cout << "Card revealed: " << index << std::endl;
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
        std::cout << "[" << nickname << "]: " << message << std::endl;
    }
}

// ==================== 기타 핸들러 ====================

void PacketHandler::HandleError(const std::string& data) {
    // ERROR
    gameState_->currentPhase = GamePhase::ERROR_PHASE;
    std::cout << "Server error occurred" << std::endl;
}
