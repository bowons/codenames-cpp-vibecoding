#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

enum class GamePhase {
    LOGIN,
    SIGNUP,
    LOBBY,
    MATCHING,
    PLAYING,
    RESULT,
    ERROR_PHASE
};

enum class PlayerRole {
    AGENT,
    SPYMASTER
};

struct Player {
    std::string nickname;
    PlayerRole role;
    bool isReady;
    int team;      // 0: RED, 1: BLUE
    bool isLeader; // 리더 여부
};

struct GameCard {
    std::string word;
    bool isRevealed;
    int cardType; // 0: red, 1: blue, 2: neutral, 3: assassin
};

struct GameMessage {
    std::string nickname;
    std::string message;
    int team;  // 0: RED, 1: BLUE, 2: SYSTEM
};

// ==================== 옵저버 인터페이스 ====================
class GameStateObserver {
public:
    virtual ~GameStateObserver() = default;
    
    virtual void OnPhaseChanged(GamePhase newPhase) {}
    virtual void OnPlayersUpdated() {}
    virtual void OnCardsUpdated() {}
    virtual void OnScoreUpdated(int redScore, int blueScore) {}
    virtual void OnHintReceived(const std::string& hint, int count) {}
    virtual void OnCardRevealed(int cardIndex) {}
    virtual void OnMessageReceived(const GameMessage& msg) {}
    virtual void OnTurnChanged(int team) {}
    virtual void OnGameOver() {}
};

class GameState {
public:
    GameState();
    
    // ==================== 상태 정보 ====================
    GamePhase currentPhase;

    std::string token;
    std::string username;
    int myPlayerIndex;      // 자신의 플레이어 인덱스
    int myRole;             // 자신의 역할 (0-3)
    int myTeam;             // 자신의 팀 (0: RED, 1: BLUE)
    bool isMyLeader;        // 자신이 리더인지 여부
    
    int sessionId;          // 게임 세션 ID
    std::vector<Player> players;
    std::vector<GameCard> cards;

    int currentTurn;        // 0: red, 1: blue
    int inGameStep;         // 게임 내부 단계(0 : 힌트, 1 : 정답) - currentPhase와 구분
    int redScore;
    int blueScore;
    std::string hintWord;
    int hintNumber;
    // 매칭 큐 관련 (서버에서 WAIT_REPLY로 전달되는 대기 인원 정보)
    int matchingCount;
    int matchingMax; // 서버가 알려주는 매칭에 필요한 최대 플레이어 수
    
    std::vector<GameMessage> messages;  // 채팅 메시지 히스토리

    // ==================== 옵저버 관리 ====================
    void AddObserver(std::shared_ptr<GameStateObserver> observer);
    void RemoveObserver(std::shared_ptr<GameStateObserver> observer);
    
    // ==================== 상태 변경 함수 ====================
    void SetPhase(GamePhase phase);
    void UpdatePlayers(const std::vector<Player>& newPlayers);
    void UpdateCards(const std::vector<GameCard>& newCards);
    void UpdateScore(int red, int blue);
    void AddMessage(const GameMessage& msg);
    void RevealCard(int cardIndex);
    void SetHint(const std::string& word, int count);
    void SetTurn(int team);
    void OnGameOver();

    void Reset();
    bool IsMyTurn() const;
    bool IsGameOver() const;

private:
    std::vector<std::shared_ptr<GameStateObserver>> observers_;
    
    // 옵저버에게 알림
    void NotifyPhaseChanged(GamePhase newPhase);
    void NotifyPlayersUpdated();
    void NotifyCardsUpdated();
    void NotifyScoreUpdated();
    void NotifyHintReceived();
    void NotifyCardRevealed(int cardIndex);
    void NotifyMessageReceived(const GameMessage& msg);
    void NotifyTurnChanged();
    void NotifyGameOver();
};