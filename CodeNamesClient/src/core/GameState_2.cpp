#include "../../include/core/GameState.h"
#include <algorithm>

// 게임 진입시 초기화

GameState::GameState()
    : currentPhase(GamePhase::LOGIN),
      myPlayerIndex(-1),
      myRole(-1),
      myTeam(-1),
      isMyLeader(false),
      sessionId(-1),
      currentTurn(0),
      currentPhase_(0),
      redScore(0),
      blueScore(0),
      hintNumber(0) {
}

// ---------------- 옵저버 관리 ---------------

// 벡터에 옵저버 추가
void GameState::AddObserver(std::shared_ptr<GameStateObserver> observer) {
    if (!observer) return;
    observers_.push_back(observer);
}

void GameState::RemoveObserver(std::shared_ptr<GameStateObserver> observer) {
    // 1. std::remove: observer와 같은 shared_ptr를 찾고, 벡터 끝으로 밀어냄
    auto newEnd = std::remove(observers_.begin(), observers_.end(), observer);    // 1. std::remove: observer와 같은 shared_ptr를 찾고, 벡터 끝으로 밀어냄

    // 2. erase: newEnd부터 실제 끝까지 삭제 (벡터 크기 줄임)
    observers_.erase(newEnd, observers_.end());

    // std에서 벡터 요소를 삭제하는 일반적인 패턴이라고 한다.
    // 특정 하나의 요소를 삭제하기 위해서는
    // auto it = std::find(v.begin(), v.end(), 2);
    // if (it != v.end()) v.erase(it);  // 첫 번째 2만 제거
}

// --------------- 상태 변경 함수 ---------------

void GameState::SetPhase(GamePhase phase) {
    if (currentPhase != phase) {
        currentPhase = phase;
        NotifyPhaseChanged(phase);
    }

    // 상태가 변경되면 Notify로 옵저버에게 알리는 전형적인 옵저버 패턴 구조
    // AI 정말 잘짠다
}

void GameState::UpdatePlayers(const std::vector<Player>& newPlayers) {
    players = newPlayers;
    NotifyPlayersUpdated();
}

void GameState::UpdateCards(const std::vector<GameCard>& newCards) {
    cards = newCards;
    NotifyCardsUpdated();
}

void GameState::UpdateScore(int red, int blue) {
    if (redScore != red || blueScore != blue) {
        redScore = red;
        blueScore = blue;
        NotifyScoreUpdated();
    }
}

void GameState::AddMessage(const GameMessage& msg) {
    messages.push_back(msg);
    NotifyMessageReceived(msg);
}

void GameState::RevealCard(int cardIndex) {
    if (cardIndex >= 0 && cardIndex < static_cast<int>(cards.size())) {
        cards[cardIndex].isRevealed = true;
        NotifyCardRevealed();
    }
}

void GameState::SetHint(const std::string& word, int count) {
    if (hintWord != word || hintNumber != count) {
        hintWord = word;
        hintNumber = count;
        NotifyHintReceived();
    }
}

void GameState::SetTurn(int team) {
    if (currentTurn != team) {
        currentTurn = team;
        NotifyTurnChanged();
    }
}

void GameState::OnGameOver() {
    currentPhase = GamePhase::RESULT;
    NotifyGameOver();
}

// --------------- 옵저버 알림 함수 ---------------
// 아래 동작들은 유니티와 언리얼의 그것과 비슷한 느낌이 듦

void GameState::NotifyPhaseChanged(GamePhase newPhase) {
    for (auto& observer : observers_) {
        if (observer) observer->OnPhaseChanged(newPhase);
    }
}

void GameState::NotifyPlayersUpdated() {
    for (auto& observer : observers_) {
        if (observer) observer->OnPlayersUpdated();
    }
}

void GameState::NotifyCardsUpdated() {
    for (auto& observer : observers_) {
        if (observer) observer->OnCardsUpdated();
    }
}

void GameState::NotifyScoreUpdated() {
    for (auto& observer : observers_) {
        if (observer) observer->OnScoreUpdated(redScore, blueScore);
    }
}

void GameState::NotifyHintReceived() {
    for (auto& observer : observers_) {
        if (observer) observer->OnHintReceived(hintWord, hintNumber);
    }
}

void GameState::NotifyCardRevealed() {
    for (auto& observer : observers_) {
        if (observer) observer->OnCardRevealed(-1);
    }
}

void GameState::NotifyMessageReceived(const GameMessage& msg) {
    for (auto& observer : observers_) {
        if (observer) observer->OnMessageReceived(msg);
    }
}

void GameState::NotifyTurnChanged() {
    for (auto& observer : observers_) {
        if (observer) observer->OnTurnChanged(currentTurn);
    }
}

void GameState::NotifyGameOver() {
    for (auto& observer : observers_) {
        if (observer) observer->OnGameOver();
    }
}

void GameState::Reset() {
    currentPhase = GamePhase::LOBBY;
    token.clear();
    username.clear();
    myPlayerIndex = -1;
    myRole = -1;
    myTeam = -1;
    isMyLeader = false;
    sessionId = -1;
    players.clear();
    cards.clear();
    currentTurn = 0;
    currentPhase_ = 0;
    redScore = 0;
    blueScore = 0;
    hintWord.clear();
    hintNumber = 0;
    messages.clear();
}
bool GameState::IsMyTurn() const {
    if (myPlayerIndex < 0 || myPlayerIndex >= static_cast<int>(players.size())) {
        return false;
    }

    if (currentPhase != GamePhase::PLAYING) {
        return false;
    }

    const Player& myPlayer = players[myPlayerIndex];
    
    // 같은 팀이 현재 턴인지 확인
    return (myPlayer.team == currentTurn);
}

bool GameState::IsGameOver() const {
    return redScore >= 9 || blueScore >= 8 || currentPhase == GamePhase::RESULT;
}
