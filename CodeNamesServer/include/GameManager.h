#pragma once

#include "Session.h"
#include "DatabaseManager.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <random>
#include <array>

enum class Team {
    RED = 0,
    BLUE = 1,
    SYSTEM = 2 // 시스템 메시지 
};

enum class PlayerRole {
    AGENT = 0, // 팀원
    SPYMASTER = 1 // 팀장
};

enum class GamePhase {
    HINT_PHASE = 0, // 팀장 힌트 단계
    GUESS_PHASE = 1 // 팀원 추측 단계
};

enum class CardType {
    RED = 1,
    BLUE = 2,
    NEUTRAL = 3,
    ASSASSIN = 4
};

// 게임 플레이어 구조체 (최소화)
struct GamePlayer {
    int roleNum;          // 0~5 (플레이어 인덱스)
    Team team;            // RED(0) or BLUE(1)
    PlayerRole role;      // AGENT(0) or SPYMASTER(1)  
    class Session* session;  // 모든 정보는 Session에서 가져옴

    std::string GetNickname() const { 
        static const std::string empty = "";
        return session ? session->GetNickname() : empty; 
    }
    std::string GetToken() const { 
        static const std::string empty = "";
        return session ? session->GetToken() : empty; 
    }
};

// 게임 카드 구조체
struct GameCard {
    std::string word;     // 카드의 단어
    CardType type;        // 카드 타입
    bool isUsed;          // 선택되었는지 여부 (기존 C의 isUsed)
};

enum class EventType {
    EVENT_NONE = 0,
    EVENT_CHAT = 1,
    EVENT_HINT = 2,
    EVENT_ANSWER = 3,
    EVENT_REPORT = 4
};

struct GameEvent {
    EventType type;
    int playerIndex;      // GamePlayer 배열 인덱스
    std::string data;     // "HINT|word|number" or "ANSWER|word" or "CHAT|msg"
};

class GameManager {
public:
    static constexpr int MAX_PLAYERS = 6; // 최대 플레이어 수
    static constexpr int MAX_CARDS = 25; // 카드 수
    static constexpr int RED_CARDS = 9; // 레드팀 카드 수
    static constexpr int BLUE_CARDS = 8; // 블루팀 카드 수
    static constexpr int NEUTRAL_CARDS = 7; // 중립 카드 수
    static constexpr int ASSASSIN_CARDS = 1; // 암살자 카드 수

private:
    std::string roomId_;
    std::array<GamePlayer, MAX_PLAYERS> players_;
    std::array<GameCard, MAX_CARDS> cards_;
    std::array<std::string, MAX_CARDS> wordList_;

    Team currentTurn_;
    GamePhase currentPhase_;

    // 게임 진행 상태
    int redScore_;
    int blueScore_;
    int remainingTries_; // 남은 추측 횟수
    std::string hintWord_; // 현재 힌트 단어
    int hintCount_;
    bool gameOver_;

    std::recursive_mutex gameMutex_;

public:
    GameManager(const std::string& roomId);
    ~GameManager();

    // 플레이어 관리
    bool AddPlayer(class Session* session, const std::string& nickname, const std::string& token);
    void RemovePlayer(const std::string& nickname);
    GamePlayer* GetPlayer(const std::string& nickname);
    GamePlayer* GetPlayerByIndex(int index);
    size_t GetPlayerCount() const;

    // 게임 초기화
    bool StartGame();
    void InitializeGame();
    bool LoadWordList(const std::string& filePath);
    void AssignCards();

    void SendAllCards(Session* session);
    void SendAllCardsToAll();

    // 게임 로직
    bool ProcessHint(int playerIndex, const std::string& word, int number);
    bool ProcessAnswer(int playerIndex, const std::string& word);
    bool ProcessChat(int playerIndex, const std::string& message);
    void SendCardUpdate(int cardIndex);

    // 턴 관리
    void SwitchTurn();
    void SwitchPhase();
    Team CheckWinner();
    void EndGame(Team winner);

    // 브로드캐스트
    void BroadcastToAll(const std::string& message);
    void BroadcastToTeam(Team team, const std::string& message);
    void BroadcastGameSystemMessage(const std::string& message);
    void SendGameInit();
    void SendGameState();

    // 게임 패킷 처리
    void HandleGamePacket(class Session* session, const std::string& data);

    // 게임 상태 조회
    Team GetCurrentTurn() const { return currentTurn_; }
    GamePhase GetCurrentPhase() const { return currentPhase_; }
    const std::string& GetRoomId() const { return roomId_; }

private:
    int FindPlayerIndex(const std::string& nickname);
    bool IsValidPlayerForHint(int playerIndex);    
    bool IsValidPlayerForAnswer(int playerIndex);
    void UpdateScores(CardType cardType);
    std::string CreateGameInitMessage();
};