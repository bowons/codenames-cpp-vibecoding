#include "../../include/core/PacketProtocol.h"
#include "../../include/gui/GameScreen.h"
#include "../../include/gui/ConsoleUtils.h"
#include <iostream>
#include <conio.h>
#include <iomanip>
#include <algorithm>

GameScreen::GameScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client)
    : gameState_(gameState),
      client_(client),
      gameRunning_(true),
      needsRedraw_(true),
      selectedCardIndex_(0),
      lastKnownRedScore_(-1),
      lastKnownBlueScore_(-1),
      lastMessageCount_(0) {
}

void GameScreen::Show() {
    ConsoleUtils::Initialize();
    
    gameRunning_ = true;
    needsRedraw_ = true;
    
    while (gameRunning_) {
        // 옵저버 콜백에서 needsRedraw_가 true로 설정될 수 있음
        if (needsRedraw_) {
            ConsoleUtils::Clear();
            ConsoleUtils::DrawBorder();
            
            // 게임 화면 구성
            DrawScoreBoard();
            DrawPlayerInfo();
            DrawCardGrid();
            DrawHintPanel();
            DrawChatPanel();
            DrawStatusBar();
            
            needsRedraw_ = false;
        }
        
        // 논블로킹 키 입력 처리 (_kbhit() = 키가 눌렸는지 확인)
        if (_kbhit()) {
            int key = _getch();
            HandleGameInput(key);
        }
        
        Sleep(100);  // 100ms 대기
    }
}

void GameScreen::DrawGameBoard() {
    ConsoleUtils::PrintCentered(1, "=== CODE NAMES - GAME ===", ConsoleColor::YELLOW);
}

void GameScreen::DrawPlayerInfo() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    int x = 2;
    int y = 3;
    
    ConsoleUtils::PrintColoredAt(x, y, "Players:", ConsoleColor::CYAN);
    
    for (int i = 0; i < gameState_->players.size() && i < 4; i++) {
        ConsoleColor color = (gameState_->players[i].team == 0) ? ConsoleColor::RED : ConsoleColor::BLUE;
        std::string marker = (gameState_->players[i].isLeader) ? "[L]" : "[A]";
        
        ConsoleUtils::PrintColoredAt(x + 2, y + i + 1, 
            marker + " " + gameState_->players[i].nickname, 
            color);
    }
}

void GameScreen::DrawScoreBoard() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    int x = width / 2 - 15;
    int y = 3;
    
    ConsoleUtils::PrintColoredAt(x, y, "RED TEAM", ConsoleColor::RED);
    ConsoleUtils::PrintColoredAt(x + 10, y, "BLUE TEAM", ConsoleColor::BLUE);
    
    ConsoleUtils::PrintColoredAt(x + 2, y + 1, 
        "Score: " + std::to_string(gameState_->redScore), 
        ConsoleColor::RED);
    ConsoleUtils::PrintColoredAt(x + 12, y + 1, 
        "Score: " + std::to_string(gameState_->blueScore), 
        ConsoleColor::BLUE);
}

void GameScreen::DrawCardGrid() {
    int x = GetCardGridStartX();
    int y = GetCardGridStartY();
    
    // 5x5 카드 그리드
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int cardIndex = row * 5 + col;
            if (cardIndex < gameState_->cards.size()) {
                const GameCard& card = gameState_->cards[cardIndex];
                
                ConsoleColor color = ConsoleColor::WHITE;
                if (card.isRevealed) {
                    if (card.cardType == 0) color = ConsoleColor::RED;
                    else if (card.cardType == 1) color = ConsoleColor::BLUE;
                    else if (card.cardType == 2) color = ConsoleColor::YELLOW;
                    else if (card.cardType == 3) color = ConsoleColor::BLACK;
                }
                
                if (selectedCardIndex_ == cardIndex) {
                    ConsoleUtils::SetTextColor(ConsoleColor::BLACK, color);
                    ConsoleUtils::PrintAt(x + col * 6, y + row * 2, 
                        "[" + card.word.substr(0, 4) + "]");
                    ConsoleUtils::ResetTextColor();
                } else {
                    ConsoleUtils::PrintColoredAt(x + col * 6, y + row * 2, 
                        "[" + card.word.substr(0, 4) + "]", color);
                }
            }
        }
    }
}

void GameScreen::DrawHintPanel() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    int x = width - 30;
    int y = 3;
    
    ConsoleUtils::PrintColoredAt(x, y, "Hint:", ConsoleColor::CYAN);
    ConsoleUtils::PrintAt(x + 1, y + 1, gameState_->hintWord);
    ConsoleUtils::PrintAt(x + 1, y + 2, "Count: " + std::to_string(gameState_->hintNumber));
}

void GameScreen::DrawChatPanel() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    int x = GetChatPanelStartX();
    int y = GetChatPanelStartY();
    
    ConsoleUtils::PrintColoredAt(x, y, "Chat:", ConsoleColor::CYAN);
    
    int messageCount = (int)gameState_->messages.size();
    int displayCount = (messageCount < 5) ? messageCount : 5;
    int startIdx = messageCount - displayCount;
    
    for (int i = 0; i < displayCount; i++) {
        const auto& msg = gameState_->messages[startIdx + i];
        ConsoleUtils::PrintAt(x + 1, y + i + 1, msg.nickname + ": " + msg.message);
    }
}

void GameScreen::DrawStatusBar() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    std::string status = "Turn: ";
    status += (gameState_->currentTurn == 0) ? "RED" : "BLUE";
    
    ConsoleUtils::PrintCentered(height - 1, status, ConsoleColor::CYAN);
}

void GameScreen::HandleGameInput(int key) {
    if (key == 27) {  // ESC
        gameRunning_ = false;
    } else if (key == 224) {  // 특수 키
        int nextKey = _getch();
        // 화살표 키로 카드 선택
        if (nextKey == 77) {  // RIGHT
            selectedCardIndex_ = (selectedCardIndex_ + 1) % 25;
        } else if (nextKey == 75) {  // LEFT
            selectedCardIndex_ = (selectedCardIndex_ - 1 + 25) % 25;
        }
    } else if (key == '\r') {  // ENTER - 카드 선택
        SelectCard(selectedCardIndex_);
    }
}

void GameScreen::SelectCard(int cardIndex) {
    if (cardIndex < gameState_->cards.size()) {
        const std::string& word = gameState_->cards[cardIndex].word;
        std::string cmd = std::string(PKT_ANSWER) + "|" + word;
        client_->SendData(cmd);
    }
}

void GameScreen::ProvideHint(const std::string& word, int count) {
    std::string cmd = std::string(PKT_HINT_MSG) + "|" + word + "|" + std::to_string(count);
    client_->SendData(cmd);
}

void GameScreen::SendMessage(const std::string& message) {
    std::string cmd = std::string(PKT_CHAT_MSG) + "|" + message;
    client_->SendData(cmd);
}

int GameScreen::GetCardGridStartX() const {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    return 2;
}

int GameScreen::GetCardGridStartY() const {
    return 9;
}

int GameScreen::GetChatPanelStartX() const {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    return width - 30;
}

int GameScreen::GetChatPanelStartY() const {
    return 12;
}

// ==================== GameStateObserver 콜백 ====================

void GameScreen::OnPhaseChanged(GamePhase newPhase) {
    // 게임 페이즈 변경 시 전체 화면 재그리기
    needsRedraw_ = true;
    std::cout << "[DEBUG] Phase changed to: " << static_cast<int>(newPhase) << std::endl;
}

void GameScreen::OnPlayersUpdated() {
    // 플레이어 정보 갱신 시 플레이어 영역 재그리기
    needsRedraw_ = true;
    std::cout << "[DEBUG] Players updated" << std::endl;
}

void GameScreen::OnCardsUpdated() {
    // 카드 정보 갱신 시 카드 그리드 재그리기
    needsRedraw_ = true;
    std::cout << "[DEBUG] Cards updated" << std::endl;
}

void GameScreen::OnScoreUpdated(int redScore, int blueScore) {
    // 점수 변경 시 점수판만 재그리기
    if (lastKnownRedScore_ != redScore || lastKnownBlueScore_ != blueScore) {
        lastKnownRedScore_ = redScore;
        lastKnownBlueScore_ = blueScore;
        needsRedraw_ = true;
        std::cout << "[DEBUG] Score updated - Red: " << redScore << ", Blue: " << blueScore << std::endl;
    }
}

void GameScreen::OnHintReceived(const std::string& hint, int count) {
    // 힌트 수신 시 힌트 패널 재그리기
    needsRedraw_ = true;
    std::cout << "[DEBUG] Hint received: " << hint << " (" << count << ")" << std::endl;
}

void GameScreen::OnCardRevealed(int cardIndex) {
    // 카드 공개 시 카드 그리드 재그리기
    needsRedraw_ = true;
    std::cout << "[DEBUG] Card revealed at index: " << cardIndex << std::endl;
}

void GameScreen::OnMessageReceived(const GameMessage& msg) {
    // 메시지 수신 시 채팅 패널 재그리기
    if (lastMessageCount_ != (int)gameState_->messages.size()) {
        lastMessageCount_ = gameState_->messages.size();
        needsRedraw_ = true;
        std::cout << "[DEBUG] Message received from " << msg.nickname << ": " << msg.message << std::endl;
    }
}

void GameScreen::OnTurnChanged(int team) {
    // 턴 변경 시 상태 표시줄 재그리기
    needsRedraw_ = true;
    std::cout << "[DEBUG] Turn changed to team: " << team << std::endl;
}

void GameScreen::OnGameOver() {
    // 게임 종료 시
    gameRunning_ = false;
    std::cout << "[DEBUG] Game over!" << std::endl;
}
