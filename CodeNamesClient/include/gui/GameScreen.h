#pragma once

#include <string>
#include <memory>
#include <vector>
#include "../core/GameState.h"
#include "../core/IOCPClient.h"

class GameScreen : public GameStateObserver, public std::enable_shared_from_this<GameScreen> {
public:
    GameScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client);
    
    // 게임 화면 표시 및 게임 루프
    void Show();
    
    // ==================== GameStateObserver 콜백 ====================
    void OnPhaseChanged(GamePhase newPhase) override;
    void OnPlayersUpdated() override;
    void OnCardsUpdated() override;
    void OnScoreUpdated(int redScore, int blueScore) override;
    void OnHintReceived(const std::string& hint, int count) override;
    void OnCardRevealed(int cardIndex) override;
    void OnMessageReceived(const GameMessage& msg) override;
    void OnTurnChanged(int team) override;
    void OnGameOver() override;

private:
    std::shared_ptr<GameState> gameState_;
    std::shared_ptr<IOCPClient> client_;
    
    bool gameRunning_;
    bool needsRedraw_;           // 재그리기 필요 여부
    int selectedCardIndex_;
    std::string inputBuffer_;
    int lastKnownRedScore_;      // 이전 점수 추적
    int lastKnownBlueScore_;
    int lastMessageCount_;       // 이전 메시지 개수 추적
    
    // 게임 화면 그리기
    void DrawGameBoard();
    void DrawPlayerInfo();
    void DrawScoreBoard();
    void DrawCardGrid();
    void DrawChatPanel();
    void DrawHintPanel();
    void DrawStatusBar();
    
    // 입력 처리
    void HandleGameInput(int key);
    void SelectCard(int cardIndex);
    void ProvideHint(const std::string& word, int count);
    void SendMessage(const std::string& message);
    
    // 레이아웃 계산
    int GetCardGridStartX() const;
    int GetCardGridStartY() const;
    int GetChatPanelStartX() const;
    int GetChatPanelStartY() const;
};
