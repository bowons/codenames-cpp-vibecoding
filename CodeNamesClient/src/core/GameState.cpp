#include "../../include/core/GameState.h"
#include <algorithm>

GameState::GameState()
    : currentPhase(GamePhase::LOGIN),
      myPlayerIndex(-1),
      myRole(-1),
      myTeam(-1),
      isMyLeader(false),
      sessionId(-1),
      currentTurn(0),
    inGameStep(0),
      redScore(0),
      blueScore(0),
      hintNumber(0) {
        matchingCount = 0;
}

// ==================== 옵저버 관리 ====================

void GameState::AddObserver(std::shared_ptr<GameStateObserver> observer) {
    if (!observer) return;
    observers_.push_back(observer);
}

void GameState::RemoveObserver(std::shared_ptr<GameStateObserver> observer) {
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end()
    );
}

// ==================== 상태 변경 함수 ====================

void GameState::SetPhase(GamePhase phase) {
    if (currentPhase != phase) {
        currentPhase = phase;
        NotifyPhaseChanged(phase);
    }
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
        NotifyCardRevealed(cardIndex);
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

// ==================== 옵저버 알림 함수 ====================

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

void GameState::NotifyCardRevealed(int cardIndex) {
    for (auto& observer : observers_) {
        if (observer) observer->OnCardRevealed(cardIndex);
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
    inGameStep = 0;
    redScore = 0;
    blueScore = 0;
    hintWord.clear();
    hintNumber = 0;
    messages.clear();
    matchingCount = 0;
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
