#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>

#include "GameManager.h"
#include "Session.h"
#include "PacketProtocol.h"
#include <ctime>

GameManager::GameManager(const std::string &roomId)
    : roomId_(roomId), currentTurn_(Team::RED), currentPhase_(GamePhase::HINT_PHASE),
      redScore_(0), blueScore_(0), remainingTries_(0), hintCount_(0), gameOver_(false) 
{
    std::cout << "GameManager 생성: " << roomId_ << std::endl;
    
    // 플레이어 배열 초기화
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        players_[i].session = nullptr;
        players_[i].roleNum = i;

        players_[i].team = (i < 3) ? Team::RED : Team::BLUE; // 0,1,2: RED, 3,4,5: BLUE
        players_[i].role = (i == 0 || i == 3) ? PlayerRole::SPYMASTER : PlayerRole::AGENT;
    }

    // 카드 설정 초기화
    for (int i = 0; i < MAX_CARDS; ++i) {
        cards_[i].word = "";
        cards_[i].type = CardType::NEUTRAL;
        cards_[i].isUsed = false;
    }

    // 단어 리스트 초기화
    for (int i = 0; i < MAX_CARDS; ++i) {
        wordList_[i] = "";
    }
}

GameManager::~GameManager() {
    std::cout << "GameManager 소멸: " << roomId_ << std::endl;

    // 게임이 진행중일 경우 모든 플레이어게 알림
    if(!gameOver_) {
        BroadcastGameSystemMessage("예기치 못하게 게임이 종료되었습니다. (서버 종료)");

        // 강제 종료 시에는 승자 없음 (-1)
        BroadcastToAll(std::string(PKT_GAME_OVER) + "|-1");
    }

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session) {
            // Session 상태를 로비로 변경
            players_[i].session->SetState(SessionState::IN_LOBBY);
            
            // GameManager 참조 해제
            players_[i].session->SetGameManager(nullptr);
            players_[i].session = nullptr;
        }
    }
}

bool GameManager::AddPlayer(Session* session, const std::string& nickname, const std::string& token) {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    if (!session) {
        std::cerr << "AddPlayer: session이 nullptr입니다." << std::endl;
        return false;
    }

    std::cout << "[" << roomId_ << "] AddPlayer 호출:" << std::endl;
    std::cout << "  Session: " << (void*)session << std::endl;
    std::cout << "  Nickname (param): '" << nickname << "'" << std::endl;
    std::cout << "  Token (param): '" << token << "'" << std::endl;
    std::cout << "  Session->GetNickname(): '" << session->GetNickname() << "'" << std::endl;
    std::cout << "  Session->GetToken(): '" << session->GetToken() << "'" << std::endl;

    // 빈 슬롯 찾기
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session == nullptr) {
            players_[i].session = session;
            // roleNum, team, role 은 생성자에서 설정

            std::cout << "플레이어 추가: " << nickname 
                     << " (슬롯 " << i 
                     << ", 팀: " << (players_[i].team == Team::RED ? "RED" : "BLUE")
                     << ", 역할: " << (players_[i].role == PlayerRole::SPYMASTER ? "SPYMASTER" : "AGENT")
                     << ", Session Nickname: '" << session->GetNickname() << "'"
                     << ")" << std::endl;
            return true;
        }
    }

    std::cerr << "AddPlayer: 빈 슬롯이 없습니다." << std::endl;
    return false;
}

void GameManager::RemovePlayer(const std::string& nickname) {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session && players_[i].session->GetNickname() == nickname) {
            std::cout << "플레이어 제거: " << nickname << " (슬롯 " << i << ")" << std::endl;
            players_[i].session->SetGameManager(nullptr);
            players_[i].session->SetState(SessionState::IN_LOBBY);
            players_[i].session = nullptr;
            return;
        }
    }
    std::cerr << "RemovePlayer: 플레이어를 찾을 수 없음: " << nickname << std::endl;
}

GamePlayer* GameManager::GetPlayer(const std::string& nickname) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session && players_[i].session->GetNickname() == nickname) {
            return &players_[i];
        }
    }

    return nullptr;
}

GamePlayer* GameManager::GetPlayerByIndex(int index) {
    if (index >= 0 && index < MAX_PLAYERS) {
        if (players_[index].session != nullptr) {
            return &players_[index];
        }
    }
    return nullptr;
}

size_t GameManager::GetPlayerCount() const {
    size_t count = 0;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session != nullptr) {
            count++;
        }
    }
    return count;
}

int GameManager::FindPlayerIndex(const std::string& nickname) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session && 
            players_[i].session->GetNickname() == nickname) {
            return i;
        }
    }
    return -1; // not found
}

bool GameManager::StartGame() {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    if (GetPlayerCount() != MAX_PLAYERS) {
        std::cerr << "StartGame: 플레이어가 부족합니다. 현재: " << GetPlayerCount() << "/6" << std::endl;
        return false;
    }

    try {
        InitializeGame();

        // Notify clients that the game is starting. Send a numeric session id (timestamp)
        // so clients waiting on GAME_START will transition to PLAYING.
        std::time_t startTs = std::time(nullptr);
        BroadcastToAll(std::string(PKT_GAME_START) + "|" + std::to_string((long long)startTs));

        SendGameInit();
        BroadcastGameSystemMessage("게임 시작!");
        SendAllCardsToAll();
        SendGameState();

        std::cout << "게임 시작: " << roomId_ << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[GameManager] StartGame threw exception: " << e.what() << std::endl;
        throw;
    }
}

void GameManager::InitializeGame() {
    // 단어 로드
    if (!LoadWordList("words.txt")) {
        std::cerr << "단어 파일 로드 실패" << std::endl;
    }

    AssignCards();

    // 상태 초기화 수행
    currentTurn_ = Team::RED;
    currentPhase_ = GamePhase::HINT_PHASE;

    redScore_ = 0;
    blueScore_ = 0;
    remainingTries_ = 0;
    
    hintWord_.clear();
    hintCount_ = 0;
    gameOver_ = false;

    std::cout << "게임 초기화 완료" << std::endl;
}

bool GameManager::LoadWordList(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "단어 파일 열기 실패: " << filePath << std::endl;
        for (int i = 0; i < MAX_CARDS; ++i) {
            wordList_[i] = "단어" + std::to_string(i + 1);
        }
        return false;
    }

    std::string line;
    int count = 0;

    while (std::getline(file, line) && count < MAX_CARDS) {
        // 줄바꿈 제거
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            wordList_[count] = line;
            count++;
        }
    }

    file.close();

    //단어 부족 체크 (실행 안됨)
    for (int i = count; i < MAX_CARDS; ++i) {
        wordList_[i] = "단어" + std::to_string(i + 1);
    }

    std::cout << "단어 파일 로드 완료: " << filePath << " (" << count << "개 단어)" << std::endl;
    return true;
}

void GameManager::AssignCards() {
    // 카드 타입 배열 
    std::vector<CardType> cardTypes;

    for (int i = 0; i < RED_CARDS; ++i) {
        cardTypes.push_back(CardType::RED);
    }

    for (int i = 0; i < BLUE_CARDS; ++i) {
        cardTypes.push_back(CardType::BLUE);
    }

    for (int i = 0; i < NEUTRAL_CARDS; ++i) {
        cardTypes.push_back(CardType::NEUTRAL);
    }

    for (int i = 0; i < ASSASSIN_CARDS; ++i) {
        cardTypes.push_back(CardType::ASSASSIN);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(cardTypes.begin(), cardTypes.end(), gen);

    for (int i = 0; i < MAX_CARDS; ++i) {
        cards_[i].word = wordList_[i];
        cards_[i].type = cardTypes[i];
        cards_[i].isUsed = false;
    }

    std::cout << "카드 배치 완료" << std::endl;
}

void GameManager::SendAllCards(Session* session) {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    std::string cardsMsg = std::string(PKT_ALL_CARDS);
    for (int i = 0; i < MAX_CARDS; ++i) {
        std::string entry = "|" + cards_[i].word + 
                           "|" + std::to_string((int)cards_[i].type) + 
                           "|" + std::to_string(cards_[i].isUsed ? 1 : 0);
        cardsMsg += entry;
    }

    if (session && !session->IsClosed()) {
        session->PostSend(cardsMsg);
        std::cout << "[" << roomId_ << "] 모든 카드 정보 전송 to " << session->GetNickname() << "[" << cardsMsg << "]" << std::endl;
    } else {
        std::cout << "[" << roomId_ << "] ALL_CARDS skipped for closed/null session" << std::endl;
    }
}

void GameManager::SendAllCardsToAll() {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session) {
            SendAllCards(players_[i].session);
        }
    }
}

void GameManager::SendCardUpdate(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= MAX_CARDS) return;

    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    // CARD_UPDATE|cardIndex|isUsed|remainingTries 형식으로 전송
    std::string updateMsg = std::string(PKT_CARD_UPDATE) + "|" + std::to_string(cardIndex) +
                            "|" + std::to_string(cards_[cardIndex].isUsed ? 1 : 0) +
                            "|" + std::to_string(remainingTries_);

    BroadcastToAll(updateMsg);
    std::cout << "[" << roomId_ << "] 카드 업데이트: " << cardIndex 
              << " (" << cards_[cardIndex].word << "), 남은 시도: " << remainingTries_ << std::endl;
}

void GameManager::BroadcastToAll(const std::string& message) {
    if (message.empty()) return;

    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session && !players_[i].session->IsClosed()) {
            players_[i].session->PostSend(message);
        }
    }
    
    std::cout << "[" << roomId_ << "] 브로드캐스트: " << message << std::endl;
}

void GameManager::BroadcastGameSystemMessage(const std::string& message) {
    std::string systemMsg = std::string(PKT_CHAT) + "|" + std::to_string((int)Team::SYSTEM) + "|0|SYSTEM|" + message;
    BroadcastToAll(systemMsg);
}

void GameManager::SendGameInit() {
    std::string initMsg = CreateGameInitMessage();
    BroadcastToAll(initMsg);

    std::cout << "[" << roomId_ << "] 게임 초기화 메시지 전송" << std::endl;
}

void GameManager::SendGameState() {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    std::string stateMsg = std::string(PKT_TURN_UPDATE) + "|" + 
                          std::to_string((int)currentTurn_) + "|" +
                          std::to_string((int)currentPhase_) + "|" +
                          std::to_string(redScore_) + "|" +
                          std::to_string(blueScore_);
                          
    BroadcastToAll(stateMsg);

     std::cout << "[" << roomId_ << "] 게임 상태 전송 - 턴: " 
              << (currentTurn_ == Team::RED ? "RED" : "BLUE") 
              << ", 단계: " << (currentPhase_ == GamePhase::HINT_PHASE ? "HINT" : "GUESS") << std::endl;
}

std::string GameManager::CreateGameInitMessage() {
    try {
        std::string msg = std::string(PKT_GAME_INIT);

        std::cout << "[" << roomId_ << "] CreateGameInitMessage - 플레이어 목록:" << std::endl;

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (players_[i].session) {
                std::string nickname;
                try {
                    // 직접 세션의 GetNickname() 호출하여 디버깅
                    Session* sess = players_[i].session;
                    std::cout << "  [" << i << "] Session pointer: " << (void*)sess << std::endl;
                    std::cout << "  [" << i << "] Calling GetNickname()..." << std::endl;
                    nickname = sess->GetNickname();
                    std::cout << "  [" << i << "] GetNickname() returned: '" << nickname << "'" << std::endl;
                } catch (...) {
                    nickname = "ERROR";
                    std::cerr << "  [" << i << "] GetNickname() failed!" << std::endl;
                }
                
                // Original C 버전과 동일하게 role_num (플레이어 인덱스 0~5), team, is_leader 전송
                int roleNum = players_[i].roleNum;  // 플레이어 인덱스 (0~5)
                int teamNum = static_cast<int>(players_[i].team);
                std::string isLeader = (players_[i].role == PlayerRole::SPYMASTER) ? "1" : "0";
                
                std::cout << "  [" << i << "] Session: " << (void*)players_[i].session 
                         << ", Nickname: '" << nickname << "'"
                         << ", RoleNum: " << roleNum
                         << ", Team: " << teamNum 
                         << ", Leader: " << isLeader << std::endl;
                
                std::string entry = "|" + nickname +
                                    "|" + std::to_string(roleNum) +
                                    "|" + std::to_string(teamNum) +
                                    "|" + isLeader;
                msg += entry;
            } else {
                std::cout << "  [" << i << "] Empty slot" << std::endl;
                std::string entry = "|" + std::string(PKT_EMPTY) + "|" + std::to_string(i) + 
                                   "|" + std::to_string((i < 3) ? 0 : 1) +
                                   "|" + std::to_string((i == 0 || i == 3) ? 1 : 0);
                msg += entry;
            }
        }

        std::cout << "[" << roomId_ << "] GAME_INIT 메시지: " << msg << std::endl;
        return msg;
    } catch (const std::exception& e) {
        std::cerr << "[" << roomId_ << "] CreateGameInitMessage exception: " << e.what() << std::endl;
        throw;
    }
}

void GameManager::BroadcastToTeam(Team team, const std::string& message) {
    if (message.empty()) return;

    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session && !players_[i].session->IsClosed() && players_[i].team == team) {
            players_[i].session->PostSend(message);
        }
    }

    std::cout << "[" << roomId_ << "] " << (team == Team::RED ? "RED" : "BLUE") << " 팀에 브로드캐스트: " << message << std::endl;
}

void GameManager::SwitchTurn() {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    currentTurn_ = (currentTurn_ == Team::RED) ? Team::BLUE : Team::RED;
    currentPhase_ = GamePhase::HINT_PHASE;

    remainingTries_ = 0;
    hintWord_.clear();
    hintCount_ = 0;

    std::cout << "[" << roomId_ << "] 턴 전환: " 
              << (currentTurn_ == Team::RED ? "RED" : "BLUE") << "팀" << std::endl;

    std::string turnMsg = std::string(PKT_TURN_UPDATE) + "|" + 
                        std::to_string((int)currentTurn_) + "|" +
                        std::to_string((int)currentPhase_) + "|" +
                        std::to_string(redScore_) + "|" +
                        std::to_string(blueScore_);

    BroadcastToAll(turnMsg);
}

void GameManager::SwitchPhase() {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    if (currentPhase_ == GamePhase::HINT_PHASE) {
        currentPhase_ = GamePhase::GUESS_PHASE;
        std::cout << "[" << roomId_ << "] 단계 전환: 추측 단계" << std::endl;
    } else {
        currentPhase_ = GamePhase::HINT_PHASE;
        std::cout << "[" << roomId_ << "] 단계 전환: 힌트 단계" << std::endl;
    }

    std::string phaseMsg = std::string(PKT_TURN_UPDATE) + "|" +
                        std::to_string((int)currentTurn_) + "|" +
                        std::to_string((int)currentPhase_) + "|" +
                        std::to_string(redScore_) + "|" +
                        std::to_string(blueScore_);

    BroadcastToAll(phaseMsg);
}

bool GameManager::ProcessHint(int playerIndex, const std::string& word, int number) {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    // 유효성 검사, 부적합시 실행 x
    if (!IsValidPlayerForHint(playerIndex)) return false;

    hintWord_ = word;
    hintCount_ = number;
    remainingTries_ = number;

    std::string hintMsg = std::string(PKT_HINT_MSG) + "|" + std::to_string((int)currentTurn_) + "|" + word + "|" + std::to_string(number);
    BroadcastToAll(hintMsg);

    SwitchPhase();
    return true;
}

bool GameManager::ProcessAnswer(int playerIndex, const std::string& word) {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    // 유효성 검사
    if (!IsValidPlayerForAnswer(playerIndex)) return false;

    int cardIndex = -1;
    for (int i = 0; i < MAX_CARDS; ++i) {
        if (cards_[i].word == word && !cards_[i].isUsed) {
            cardIndex = i;
            break;
        }
    }

    if (cardIndex == -1) {
    // 잘못된 단어 - 해당 플레이어에게만 고지함
    players_[playerIndex].session->PostSend(std::string(PKT_ANSWER_RESULT) + "|INVALID|" + word);
        return false;
    }

    cards_[cardIndex].isUsed = true;
    CardType cardType = cards_[cardIndex].type;

    std::string playerName = players_[playerIndex].GetNickname();

    bool turnEnds = false;
    bool gameEnds = false;
    std::string chatMsg;

    if (cardType == CardType::RED) {
        redScore_++;
        if (currentTurn_ == Team::RED) {
            remainingTries_--;
            chatMsg = std::string(PKT_CHAT) + "|2|0|시스템|" + playerName + "님이 RED 카드를 선택! (+1점)";
            if (remainingTries_ <= 0) turnEnds = true;
        } else {
            turnEnds = true;
            chatMsg = std::string(PKT_CHAT) + "|2|0|시스템|" + playerName + "님이 RED 카드를 선택! 턴 종료.";
        }
    } else if (cardType == CardType::BLUE) {
        blueScore_++;
        if (currentTurn_ == Team::BLUE) {
            remainingTries_--;
            chatMsg = std::string(PKT_CHAT) + "|2|0|시스템|" + playerName + "님이 BLUE 카드를 선택! (+1점)";
            if (remainingTries_ <= 0) turnEnds = true;
        } else {
            turnEnds = true;
            chatMsg = std::string(PKT_CHAT) + "|2|0|시스템|" + playerName + "님이 BLUE 카드를 선택! 턴 종료.";
        }
    } else if (cardType == CardType::NEUTRAL) {
        // 중립 카드 선택 시 점수 변화 없이 턴만 종료 (코드네임 규칙)
        chatMsg = std::string(PKT_CHAT) + "|2|0|시스템|" + playerName + "님이 중립 카드를 선택! 턴 종료.";
        turnEnds = true;
    } else if (cardType == CardType::ASSASSIN) {
        gameEnds = true;
        chatMsg = std::string(PKT_CHAT) + "|2|0|시스템|" + playerName + "님이 암살자를 선택! 게임 종료.";
    }
    
    // remainingTries 계산 후 CARD_UPDATE 전송
    SendCardUpdate(cardIndex);
    BroadcastToAll(chatMsg);

    Team winner = CheckWinner();
    if (winner != Team::SYSTEM || gameEnds) {
        if (cardType == CardType::ASSASSIN) {
            Team assassinWinner = (currentTurn_ == Team::RED) ? Team::BLUE : Team::RED;
            EndGame(assassinWinner);
        } else {
            EndGame(winner);
        }
        return true;
    }

    if (turnEnds)
        SwitchTurn();
    
    return true;
}

bool GameManager::ProcessChat(int playerIndex, const std::string &message)
{
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    if (gameOver_) return false;
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return false;
    if (players_[playerIndex].session == nullptr) return false;
    if (players_[playerIndex].session->IsClosed()) return false;
    if (message.empty()) return false;

    try {
        std::string playerName = players_[playerIndex].GetNickname();
        Team playerTeam = players_[playerIndex].team;

        std::string chatMsg = std::string(PKT_CHAT) + "|" + 
                            std::to_string((int)playerTeam) + "|" + 
                            std::to_string(playerIndex) + "|" + 
                            playerName + "|" + 
                            message;

        BroadcastToAll(chatMsg);

        std::cout << "[" << roomId_ << "] 채팅 from " << playerName << ": " << message << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[" << roomId_ << "] ProcessChat exception: " << e.what() << std::endl;
        return false;
    }
}

Team GameManager::CheckWinner() {
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    if (redScore_ >= RED_CARDS) {
        std::cout << "[" << roomId_ << "] RED팀 승리! (점수: " << redScore_ << "/" << RED_CARDS << ")" << std::endl;
        return Team::RED;
    }

    if (blueScore_ >= BLUE_CARDS) {
        std::cout << "[" << roomId_ << "] BLUE팀 승리! (점수: " << blueScore_ << "/" << BLUE_CARDS << ")" << std::endl;
        return Team::BLUE;
    }
    return Team::SYSTEM; // 승자 없음
}

void GameManager::EndGame(Team winner)
{
    std::lock_guard<std::recursive_mutex> lock(gameMutex_);

    gameOver_ = true;
    std::string winnerName = (winner == Team::RED) ? "RED" : 
                            (winner == Team::BLUE) ? "BLUE" : "DRAW";
    BroadcastGameSystemMessage(winnerName + "팀이 승리했습니다!");

    std::string gameOverMsg = std::string(PKT_GAME_OVER) + "|" + std::to_string((int)winner);
    BroadcastToAll(gameOverMsg);

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session) {
            std::string nickname = players_[i].GetNickname();
            std::string result = (players_[i].team == winner) ? "WIN" : "LOSS";
            
            // DatabaseManager 싱글톤을 통해 결과 저장
            try {
                DatabaseResult dbResult = DatabaseManager::GetInstance().SaveGameResult(nickname, result);
                if (dbResult == DatabaseResult::SUCCESS) {
                    std::cout << "[" << roomId_ << "] 게임 결과 저장 성공: " 
                             << nickname << " - " << result << std::endl;
                } else {
                    std::cerr << "[" << roomId_ << "] 게임 결과 저장 실패: " 
                             << nickname << " - " << result << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[" << roomId_ << "] DB 접근 예외: " << e.what() << std::endl;
            }
        }
    }

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].session) {
            players_[i].session->SetState(SessionState::IN_LOBBY);
            players_[i].session->SetGameManager(nullptr);
        }
    }

    std::cout << "[" << roomId_ << "] 게임 종료: " << winnerName << "팀 승리" << std::endl;

}


void GameManager::HandleGamePacket(Session* session, const std::string& data)
{
    if (!session || session->IsClosed() || data.empty()) {
        std::cerr << "HandleGamePacket: 유효하지 않은 세션 또는 데이터" << std::endl;
        return;
    }

    int playerIndex = FindPlayerIndex(session->GetNickname());
    if (playerIndex == -1) {
        std::cerr << "HandleGamePacket: 플레이어 인덱스를 찾을 수 없음: " << session->GetNickname() << std::endl;
        return;
    }
    
    if (data.find(std::string(PKT_HINT_MSG) + "|") == 0) { // "HINT|단어|숫자"
        std::string params = data.substr(std::string(PKT_HINT_MSG).size() + 1);
        
        size_t pos = params.find('|');
        if (pos != std::string::npos) {
            std::string word = params.substr(0, pos); // |단어
            std::string numberStr = params.substr(pos + 1); // |숫자

            try { 
                int number = std::stoi(numberStr);
                ProcessHint(playerIndex, word, number); 
            } catch (const std::exception& e) {
                std::cerr << "HandleGamePacket: 숫자 파싱 오류: " << numberStr << std::endl;
            }
        }
    } else if (data.find(std::string(PKT_ANSWER) + "|") == 0) { // "ANSWER|단어"
        std::string word = data.substr(std::string(PKT_ANSWER).size() + 1);
        ProcessAnswer(playerIndex, word);
    } else if (data.find(std::string(PKT_CHAT) + "|") == 0) { // "CHAT|메시지"
        std::string message = data.substr(std::string(PKT_CHAT).size() + 1);
        ProcessChat(playerIndex, message);
    } else {
        std::cerr << "HandleGamePacket: 알 수 없는 패킷 타입: " << data << std::endl;
    }

    return;
}

bool GameManager::IsValidPlayerForHint(int playerIndex)
{
    
    if (gameOver_) return false;
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return false;
    if (players_[playerIndex].session == nullptr) return false;
    if (players_[playerIndex].session->IsClosed()) return false;
    if (players_[playerIndex].team != currentTurn_) return false;
    if (currentPhase_ != GamePhase::HINT_PHASE) return false;
    if (players_[playerIndex].role != PlayerRole::SPYMASTER) return false;

    return true;
}

bool GameManager::IsValidPlayerForAnswer(int playerIndex)
{
    if (gameOver_) return false;
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return false;
    if (players_[playerIndex].session == nullptr) return false;
    if (players_[playerIndex].session->IsClosed()) return false;
    if (players_[playerIndex].role != PlayerRole::AGENT) return false;
    if (players_[playerIndex].team != currentTurn_) return false;
    if (currentPhase_ != GamePhase::GUESS_PHASE) return false;
    if (remainingTries_ <= 0) return false;
    
    return true;
}