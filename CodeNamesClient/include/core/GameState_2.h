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
    int team;
    bool isLeader;
};

struct GameCard {
    std::string word;
    bool isRevealed;
    int cardType; // 0: red, 1: lue, 2: netural, 3: assasin
};

struct GameMessage {
    std::string nickname;
    std::string message;
    int team;  // 0: RED, 1: BLUE, 2: SYSTEM
};

// 여기서부터는 AI가 작성한 옵저버 패턴

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

    // 좀 더 엔진에서의 이벤트 형태를 닮은 것 같음
    // 기존 C 코드에 비해 구조적이고 훨씬 간결해 보인다

};

class GameState {
public:
    GameState();

    // ---------------------상태 정보 ------------------------
    GamePhase currentPhase;

    std::string token;
    std::string username;
    int myPlayerIndex; // 내 플레이어 인덱스
    int myRole; // 자신의 역할
    int myTeam;
    bool isMyLeader; // 자신이 리더인지 여부

    int sessionId;
    std::vector<Player> players;
    std::vector<GameCard> cards;

    int currerntTurn; // 0 : RED, 1: BLUE
    int inGameStep; // 0 : 힌트, 1: 정답
    int redScore;
    int blueScore;
    std::string hintWord;
    int hintNumber;

    // 옵저버 관리
    void AddObserver(std::shared_ptr<GameStateObserver> observer);
    void RemoveObserver(std::shared_ptr<GameStateObserver> observer);

    // 왜 shared_ptr를 쓰는가?
    // --> 하나의 옵저버를 여러 객체에서 참조할 때, 안전하게 관리하기 위해서

    void SetPhase(GamePhase phase);
    void UpdatePlayers(const std::vector<Player>& newPlayers);
    void UpdateCards(const std::vector<GameCard>& newCards);
    void UpdateScore(int red, int blue);
    void AddMessage(const GameMessage& msg);
    void UpdateHint(const std::string& word, int number);
    void RevealCard(int cardIndex);
    void SetHint(const std::string& word, int count);
    void SetTurn(int team);
    void OnGameOver();

    void Reset();
    bool IsMyTurn() const;
    bool IsGameOver() const;

private:
    // 옵저버들을 추가한다.
    std::vector<std::shared_ptr<GameStateObserver>> observers_;

    // 알림
    void NotifyPhaseChanged(GamePhase newPhase);
    void NotifyPlayersUpdated();
    void NotifyCardsUpdated();
    void NotifyScoreUpdated();
    void NotifyHintReceived();
    void NotifyCardRevealed();
    void NotifyMessageReceived(const GameMessage& msg);
    void NotifyTurnChanged();
    void NotifyGameOver();
};