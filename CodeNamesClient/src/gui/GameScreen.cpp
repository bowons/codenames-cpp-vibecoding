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
      lastMessageCount_(0),
      inputMode_(NONE),
      inInputMode_(false) {
    gameState_->AddObserver(shared_from_this());
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
    
    std::string status;
    
    // 입력 모드에 따라 다른 상태 표시
    if (inputMode_ == INPUT_HINT_WORD) {
        status = "힌트 단어를 입력하세요 (엔터로 완료):";
    } else if (inputMode_ == INPUT_HINT_COUNT) {
        status = "연관된 카드 수를 입력하세요 (엔터로 완료):";
    } else if (inputMode_ == INPUT_ANSWER) {
        status = "답변을 입력하세요 (엔터로 완료):";
    } else if (inputMode_ == INPUT_CHAT) {
        status = "채팅을 입력하세요 (엔터로 전송, TAB으로 게임 모드로 전환):";
    } else {
        // 일반 상태 표시
        status = "Turn: ";
        status += (gameState_->currentTurn == 0) ? "RED" : "BLUE";
        status += " | Phase: ";
        status += (gameState_->inGameStep == 0) ? "HINT" : "ANSWER";
        status += " | TAB:모드전환 ESC:종료";
    }
    
    ConsoleUtils::PrintCentered(height - 1, status, ConsoleColor::CYAN);
}

void GameScreen::HandleGameInput(int key) {
    // C 코드와 동일한 입력 처리 로직
    if (key == 27) {  // ESC
        if (inputMode_ != NONE) {
            // 입력 모드 취소
            inputMode_ = NONE;
            inInputMode_ = false;
            needsRedraw_ = true;
            Logger::Info("Input mode cancelled");
        } else {
            // 게임 종료
            gameRunning_ = false;
        }
    } else if (key == 9) {  // TAB - C 코드와 동일하게 모드 전환
        if (inputMode_ == INPUT_CHAT) {
            // 채팅 모드에서 게임 입력 모드로 전환 (C 코드와 동일한 로직)
            UpdateInputModeBasedOnGameState();
            Logger::Info("TAB: CHAT → GAME_MODE");
        } else {
            // 게임 입력 모드에서 채팅 모드로 전환
            inputMode_ = INPUT_CHAT;
            inInputMode_ = true;
            needsRedraw_ = true;
            Logger::Info("TAB: GAME_MODE → CHAT");
        }
    } else if (key == '\r') {  // ENTER
        // 현재 입력 모드에 따라 처리
        if (inputMode_ == INPUT_HINT_WORD) {
            // 힌트 단어 입력 완료, 숫자 입력 모드로 전환
            int width, height;
            ConsoleUtils::GetConsoleSize(width, height);
            int inputWidth = 60;
            ConsoleUtils::DrawInputBox(2, height - 3, inputWidth, "Hint word: ");
            hintWord_ = ConsoleUtils::GetInput(20, false);
            if (!hintWord_.empty()) {
                inputMode_ = INPUT_HINT_COUNT;  // 다음 단계로 전환
                needsRedraw_ = true;
                Logger::Info("Hint word entered: " + hintWord_);
            }
        } else if (inputMode_ == INPUT_HINT_COUNT) {
            // 힌트 숫자 입력 완료, 힌트 전송
            int width, height;
            ConsoleUtils::GetConsoleSize(width, height);
            int inputWidth = 60;
            ConsoleUtils::DrawInputBox(2, height - 3, inputWidth, "Number of cards: ");
            std::string countStr = ConsoleUtils::GetInput(3, false);
            int count = 0;
            try { count = std::stoi(countStr); } catch (...) { count = 0; }
            
            if (count > 0) {
                ProvideHint(hintWord_, count);
                Logger::Info("Hint sent: " + hintWord_ + " (" + std::to_string(count) + ")");
            }
            
            // 입력 모드 종료 (서버가 자동으로 ANSWER Phase로 전환)
            inputMode_ = NONE;
            inInputMode_ = false;
            needsRedraw_ = true;
            ConsoleUtils::ClearLine(2, height - 3, inputWidth);
        } else if (inputMode_ == INPUT_ANSWER) {
            // 답변 입력 완료, 답변 전송
            int width, height;
            ConsoleUtils::GetConsoleSize(width, height);
            int inputWidth = 60;
            ConsoleUtils::DrawInputBox(2, height - 3, inputWidth, "Answer: ");
            std::string answer = ConsoleUtils::GetInput(20, false);
            if (!answer.empty()) {
                // 답변 전송
                std::string cmd = std::string(PKT_ANSWER) + "|" + answer;
                client_->SendData(cmd);
                Logger::Info("Answer sent: " + answer);
            }
            
            // 입력 모드 종료
            inputMode_ = NONE;
            inInputMode_ = false;
            needsRedraw_ = true;
            ConsoleUtils::ClearLine(2, height - 3, inputWidth);
        } else if (inputMode_ == INPUT_CHAT) {
            // 채팅 입력 완료, 채팅 전송
            int width, height;
            ConsoleUtils::GetConsoleSize(width, height);
            int inputWidth = 80;
            ConsoleUtils::DrawInputBox(2, height - 3, inputWidth, "Chat: ");
            std::string msg = ConsoleUtils::GetInput(64, false);
            if (!msg.empty()) {
                SendMessage(msg);
                needsRedraw_ = true;
                Logger::Info("Chat sent: " + msg);
            }
            // 채팅 모드 유지 (C 코드처럼)
            ConsoleUtils::ClearLine(2, height - 3, inputWidth);
        }
    }
}

void GameScreen::SelectCard(int cardIndex) {
    if (cardIndex < gameState_->cards.size()) {
        const std::string& word = gameState_->cards[cardIndex].word;
        std::string cmd = "ANSWER|" + word;
        client_->SendData(cmd);
    }
}

void GameScreen::ProvideHint(const std::string& word, int count) {
    std::string cmd = "HINT|" + word + "|" + std::to_string(count);
    client_->SendData(cmd);
}

void GameScreen::SendMessage(const std::string& message) {
    std::string cmd = "CHAT|" + message;
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
    
    // 페이즈 변경 시 입력 모드 자동 설정
    if (newPhase == GamePhase::PLAYING) {
        UpdateInputModeBasedOnGameState();
    } else {
        inputMode_ = NONE;
        inInputMode_ = false;
    }
    
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
    // 턴 변경 시 상태 표시줄 재그리기 및 입력 모드 업데이트
    needsRedraw_ = true;
    UpdateInputModeBasedOnGameState();
    std::cout << "[DEBUG] Turn changed to team: " << team << std::endl;
}

void GameScreen::OnGameOver() {
    // 게임 종료 시
    gameRunning_ = false;
    std::cout << "[DEBUG] Game over!" << std::endl;
}

// ==================== 입력 모드 관리 ====================

void GameScreen::UpdateInputModeBasedOnGameState() {
    // C 코드와 동일한 로직으로 입력 모드 자동 설정
    if (gameState_->inGameStep == 0 && gameState_->isMyLeader && gameState_->myTeam == gameState_->currentTurn) {
        // HINT Phase && 내가 리더 && 내 팀 턴: 힌트 입력 모드
        inputMode_ = INPUT_HINT_WORD;
        inInputMode_ = true;
        std::cout << "[DEBUG] Auto-entered HINT input mode (leader turn)" << std::endl;
    } else if (gameState_->inGameStep == 1 && !gameState_->isMyLeader && gameState_->myTeam == gameState_->currentTurn) {
        // ANSWER Phase && 내가 팀원 && 내 팀 턴: 답변 입력 모드
        inputMode_ = INPUT_ANSWER;
        inInputMode_ = true;
        std::cout << "[DEBUG] Auto-entered ANSWER input mode (team member turn)" << std::endl;
    } else {
        // 그 외: 입력 모드 해제
        inputMode_ = NONE;
        inInputMode_ = false;
        std::cout << "[DEBUG] Input mode disabled (not my turn)" << std::endl;
    }
    needsRedraw_ = true;
}
