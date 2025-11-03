#include "../../include/gui/GameScreen.h"
#include "../../include/gui/ConsoleUtils.h"
#include "../../include/core/Logger.h"
#include "../../include/core/PacketProtocol.h"
#include "../../include/core/PacketHandler.h"
#include <iostream>
#include <conio.h>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <deque>

// 전역 패킷 큐 (main.cpp에서 정의됨)
extern std::mutex g_packetQueueMutex;
extern std::deque<std::string> g_packetQueue;

// UTF-8 helpers: count code points and safely truncate without cutting multi-byte sequences
static int Utf8Length(const std::string& s) {
    int len = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        // continuation bytes have the two highest bits == 10 (0x80..0xBF)
        if ((c & 0xC0) != 0x80) {
            ++len;
        }
    }
    return len;
}

static std::string Utf8Truncate(const std::string& s, int maxChars) {
    if (maxChars <= 0) return std::string();
    int count = 0;
    size_t pos = 0;
    for (; pos < s.size(); ++pos) {
        unsigned char c = static_cast<unsigned char>(s[pos]);
        if ((c & 0xC0) != 0x80) {
            if (count == maxChars) break;
            ++count;
        }
    }
    return s.substr(0, pos);
}

static std::string Utf8Pad(const std::string& s, int targetChars) {
    int cur = Utf8Length(s);
    if (cur >= targetChars) return s;
    return s + std::string(targetChars - cur, ' ');
}

GameScreen::GameScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client)
    : gameState_(gameState),
      client_(client),
      gameRunning_(false),
      needsRedraw_(true),
      selectedCardIndex_(0),
      lastKnownRedScore_(-1),
      lastKnownBlueScore_(-1),
      lastMessageCount_(0),
      inputMode_(NONE),
      inputThreadRunning_(false),
      inputEchoNeedsUpdate_(false) {
    // Registration is done by GUIManager after construction to avoid
    // calling shared_from_this() inside the constructor (would throw bad_weak_ptr).
}

GameScreen::~GameScreen() {
    // 게임 루프 종료
    gameRunning_ = false;
    
    // 입력 스레드 종료
    if (inputThreadRunning_) {
        inputThreadRunning_ = false;
        if (inputThread_.joinable()) {
            inputThread_.join();
        }
    }
}

void GameScreen::Show() {
    ConsoleUtils::Initialize();
    
    gameRunning_ = true;
    needsRedraw_ = true;
    UpdateInputModeBasedOnGameState();  // 초기 입력 모드 설정
    
    // ===== 입력 스레드 시작 =====
    inputThreadRunning_ = true;
    inputThread_ = std::thread(&GameScreen::InputThreadFunc, this);
    
    // ===== 메인 렌더링 루프 =====
    while (gameRunning_) {
        // 0. 게임 종료 확인 (RESULT 페이즈로 전환되면 게임 화면 종료)
        if (gameState_->currentPhase == GamePhase::RESULT) {
            Logger::Info("Game ended - transitioning to RESULT screen");
            gameRunning_ = false;
            break;
        }
        
        // 1. 패킷 큐 드레인 (네트워크 메시지 즉시 처리)
        DrainPacketQueue();
        
        // 2. 입력 이벤트 처리 (입력 스레드에서 온 이벤트들)
        ProcessInputEvents();
        
        // 3. 화면 재그리기
        if (needsRedraw_) {
            ConsoleUtils::Clear();
            ConsoleUtils::DrawBorder();
            
            // 게임 화면 구성
            DrawScoreBoard();      // 타이틀
            DrawHintPanel();       // 힌트 (상단)
            DrawPlayerInfo();      // 플레이어 목록 (좌측)
            DrawCardGrid();        // 카드 그리드 (중앙)
            DrawChatPanel();       // 채팅 (하단)
            DrawStatusBar();       // 상태 바 (최하단)
            
            needsRedraw_ = false;
            inputEchoNeedsUpdate_ = false;
        } else if (inputEchoNeedsUpdate_) {
            // 전체 화면이 아닌 입력 에코만 갱신
            DrawStatusBar();
            inputEchoNeedsUpdate_ = false;
        }
        
        // 4. CPU 부하 감소 (10ms = 100fps)
        Sleep(10);
    }
    
    // ===== 입력 스레드 종료 대기 =====
    inputThreadRunning_ = false;
    if (inputThread_.joinable()) {
        inputThread_.join();
    }
}

// ===== 입력 스레드 함수 (별도 스레드에서 실행) =====
void GameScreen::InputThreadFunc() {
    while (inputThreadRunning_) {
        
        // 키 입력 대기 (블로킹)
        wint_t wch = _getwch();
        
        // 입력 이벤트 생성 및 큐에 추가
        InputEvent event(InputEvent::KEY_CHAR, wch);
        
        // 특수 키 분류
        if (wch == 27) {
            event.type = InputEvent::KEY_ESC;
        } else if (wch == L'\t') {
            event.type = InputEvent::KEY_TAB;
        } else if (wch == L'\r' || wch == L'\n') {
            event.type = InputEvent::KEY_ENTER;
        } else if (wch == L'\b') {
            event.type = InputEvent::KEY_BACKSPACE;
        }
        
        // 이벤트 큐에 추가 (스레드 안전)
        {
            std::lock_guard<std::mutex> lock(inputEventMutex_);
            inputEvents_.push_back(event);
        }
        
        // ===== 입력 즉시 redraw 플래그 설정 =====
        // 문자 입력(CHAR/BACKSPACE)이면 즉시 echo 갱신 트리거
        if (event.type == InputEvent::KEY_CHAR || event.type == InputEvent::KEY_BACKSPACE) {
            inputEchoNeedsUpdate_ = true;
        }
        
        // ===== Sleep(1) 제거 - 블로킹 방식이므로 불필요 =====
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
    
    // 레드팀 표시
    int redRemaining = 9 - gameState_->redScore;
    ConsoleUtils::PrintColoredAt(x, y, "레드 팀      남은 단어 = " + std::to_string(redRemaining), ConsoleColor::RED);
    
    // 레드팀 플레이어 (0, 1, 2)
    for (int i = 0; i < 3 && i < gameState_->players.size(); i++) {
        std::string marker = (gameState_->players[i].isLeader) ? "[스파이마스터]" : "[요원]";
        std::string playerText = "  " + gameState_->players[i].nickname + "     " + marker;
        ConsoleUtils::PrintColoredAt(x, y + 1 + i, playerText, ConsoleColor::RED);
    }
    
    // 블루팀 표시
    int blueRemaining = 8 - gameState_->blueScore;
    ConsoleUtils::PrintColoredAt(x, y + 5, "블루 팀      남은 단어 = " + std::to_string(blueRemaining), ConsoleColor::BLUE);
    
    // 블루팀 플레이어 (3, 4, 5)
    for (int i = 3; i < 6 && i < gameState_->players.size(); i++) {
        std::string marker = (gameState_->players[i].isLeader) ? "[스파이마스터]" : "[요원]";
        std::string playerText = "  " + gameState_->players[i].nickname + "     " + marker;
        ConsoleUtils::PrintColoredAt(x, y + 3 + i, playerText, ConsoleColor::BLUE);
    }
}

void GameScreen::DrawScoreBoard() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    // 화면 상단 중앙에 표시
    std::string title = "=== CODE NAMES ===";
    ConsoleUtils::PrintCentered(1, title, ConsoleColor::YELLOW);
    
    // 점수는 플레이어 정보에 포함되어 있으므로 별도 표시하지 않음
}

void GameScreen::DrawCardGrid() {
    int x = GetCardGridStartX();
    int y = GetCardGridStartY();
    
    // 박스 형태로 카드 렌더링 (약간 더 컴팩트하게)
    const int cardWidth = 10;    // +--------+ (10글자)
    const int cardHeight = 3;    // 3줄 (상단, 중간, 하단)
    const int colSpacing = 11;   // 카드 간 간격 (12 → 11로 줄임)
    const int rowSpacing = 3;    // 행 간 간격 (4 → 3으로 줄임)

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int cardIndex = row * 5 + col;
            if (cardIndex < gameState_->cards.size()) {
                const GameCard& card = gameState_->cards[cardIndex];

                int cardX = x + col * colSpacing;
                int cardY = y + row * rowSpacing;

                // 색상 결정
                ConsoleColor color = ConsoleColor::WHITE;

                // 스파이마스터는 모든 카드의 색상을 볼 수 있음
                // 일반 요원은 공개된 카드만 색상 표시
                if (card.isRevealed || gameState_->isMyLeader) {
                    // 서버 CardType: RED=1, BLUE=2, NEUTRAL=3, ASSASSIN=4
                    if (card.cardType == 1) color = ConsoleColor::RED;        // RED 카드
                    else if (card.cardType == 2) color = ConsoleColor::BLUE;  // BLUE 카드
                    else if (card.cardType == 3) color = ConsoleColor::WHITE; // 중립 카드
                    else if (card.cardType == 4) color = ConsoleColor::YELLOW; // 암살자 카드 (주황색 의도)
                }

                // 카드 박스 그리기
                ConsoleUtils::PrintColoredAt(cardX, cardY,     "+--------+", color);
                ConsoleUtils::PrintColoredAt(cardX, cardY + 1, "|        |", color);
                ConsoleUtils::PrintColoredAt(cardX, cardY + 2, "+--------+", color);

                // 카드 텍스트 표시 (중앙 정렬)
                std::string displayWord;
                if (card.isRevealed) {
                    displayWord = "완료";  // "완료" 표시
                } else {
                    displayWord = Utf8Truncate(card.word, 4);
                }

                int textLen = Utf8Length(displayWord);
                int padding = (8 - textLen) / 2;
                ConsoleUtils::PrintColoredAt(cardX + 1 + padding, cardY + 1, displayWord, color);
            }
        }
    }
}

void GameScreen::DrawHintPanel() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    // 카드 그리드 바로 위에 중앙 정렬로 표시
    int cardGridX = GetCardGridStartX();
    int y = 1;  // 최상단
    
    // 현재 힌트가 있으면 표시
    if (!gameState_->hintWord.empty()) {
        std::string hintDisplay = "[힌트] " + gameState_->hintWord + " (연관 카드: " + std::to_string(gameState_->hintNumber) + "개)";
        ConsoleUtils::PrintColoredAt(cardGridX, y, hintDisplay, ConsoleColor::YELLOW);
        
        // ANSWER 단계(inGameStep == 1)일 때 남은 시도 횟수 표시
        if (gameState_->inGameStep == 1 && gameState_->remainingTries > 0) {
            std::string triesDisplay = " [남은 시도: " + std::to_string(gameState_->remainingTries) + "회]";
            ConsoleUtils::PrintColoredAt(cardGridX, y + 1, triesDisplay, ConsoleColor::CYAN);
        }
    } else {
        // 힌트 대기중일 때는 표시하지 않음 (깔끔하게)
    }
}

void GameScreen::DrawChatPanel() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    int x = 2;
    int y = 20;  // 위로 올림 (24 → 20)
    
    ConsoleUtils::PrintColoredAt(x, y, "-------------------<채팅 로그>-------------------", ConsoleColor::CYAN);
    
    int messageCount = (int)gameState_->messages.size();
    int displayCount = (std::min)(8, messageCount);  // 최대 8줄
    int startIdx = (messageCount > displayCount) ? (messageCount - displayCount) : 0;
    
    for (int i = 0; i < displayCount; i++) {
        const auto& msg = gameState_->messages[startIdx + i];
        std::string displayMsg = msg.nickname + ": " + msg.message;
        
        // 메시지 타입에 따라 색상 변경
        ConsoleColor msgColor = ConsoleColor::WHITE;
        if (msg.team == 2) {  // SYSTEM 메시지
            msgColor = ConsoleColor::YELLOW;
        }
        
        ConsoleUtils::PrintColoredAt(x, y + 1 + i, displayMsg, msgColor);
    }
    
    // 안내 메시지
    ConsoleUtils::PrintColoredAt(x, y + 10, "Tab: 채팅 전환", ConsoleColor::GRAY);
}

void GameScreen::DrawStatusBar() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    // 입력 가이드라인 위치: 채팅창 바로 아래 (y=31)
    int inputGuideY = 31;
    
    // 실시간 입력 echo 위치: 최하단 상태바 옆 (y=38)
    int inputEchoY = height - 2;
    
    // 상태바 위치: 최하단 (y=39)
    int statusY = height - 1;
    
    // 먼저 입력 영역을 깨끗이 지우기
    ConsoleUtils::ClearLine(0, inputGuideY, width);
    ConsoleUtils::ClearLine(0, inputEchoY, width);
    
    // 입력 모드일 때 가이드라인과 실시간 echo 표시
    if (inputMode_ != NONE) {
        std::string guide;
        
        if (inputMode_ == INPUT_HINT_WORD) {
            guide = "→ 힌트 단어를 입력하세요 (엔터로 완료):";
        } else if (inputMode_ == INPUT_HINT_COUNT) {
            guide = "→ 연관된 카드 수를 입력하세요 (엔터로 완료):";
        } else if (inputMode_ == INPUT_ANSWER) {
            guide = "→ 단어를 입력하세요 (엔터로 완료):";
        } else if (inputMode_ == INPUT_CHAT) {
            guide = "→ 채팅을 입력하세요 (엔터로 전송):";
        }
        
        // 입력 가이드라인 표시 (채팅창 아래)
        ConsoleUtils::PrintColoredAt(2, inputGuideY, guide, ConsoleColor::CYAN);
        
        // 실시간 입력 echo (상태바 바로 위, y=38)
        std::string echoPrompt = "입력: ";
        ConsoleUtils::PrintAt(2, inputEchoY, echoPrompt);
        ConsoleUtils::PrintColoredAt(2 + (int)echoPrompt.length(), inputEchoY, inputBuffer_ + "_", ConsoleColor::YELLOW);
    }
    
    // 상태 바 표시 (최하단, y=39)
    std::string status;
    if (inputMode_ != NONE) {
        status = "TAB:채팅전환 | ESC:취소";
    } else {
        status = "Turn: ";
        status += (gameState_->currentTurn == 0) ? "RED" : "BLUE";
        status += " | Phase: ";
        status += (gameState_->inGameStep == 0) ? "HINT" : "ANSWER";
        status += " | TAB:채팅 | ESC:종료";
    }
    
    ConsoleUtils::PrintAt(2, statusY, status);
}

// ===== 이전 레거시 함수들 제거됨 (스레드 기반으로 교체) =====

// ===== 입력 이벤트 처리 (메인 스레드) =====
void GameScreen::ProcessInputEvents() {
    std::vector<InputEvent> events;
    
    // 이벤트 큐에서 모든 대기 중인 이벤트를 가져옴 (스레드 안전)
    {
        std::lock_guard<std::mutex> lock(inputEventMutex_);
        if (inputEvents_.empty()) return;
        
        events.reserve(inputEvents_.size());
        for (const auto& ev : inputEvents_) {
            events.push_back(ev);
        }
        inputEvents_.clear();
    }
    
    // 모든 이벤트 처리
    for (const auto& event : events) {
        HandleInputEvent(event);
    }
}

void GameScreen::HandleInputEvent(const InputEvent& event) {
    switch (event.type) {
        case InputEvent::KEY_ESC:
            if (inputMode_ != NONE) {
                inputMode_ = NONE;
                inputBuffer_.clear();
                needsRedraw_ = true;
                Logger::Info("Input mode cancelled");
            } else {
                gameRunning_ = false;
            }
            break;
            
        case InputEvent::KEY_TAB:
            if (inputMode_ == INPUT_CHAT) {
                inputBuffer_.clear();
                UpdateInputModeBasedOnGameState();
                Logger::Info("TAB: CHAT → GAME_MODE");
            } else {
                inputBuffer_.clear();
                inputMode_ = INPUT_CHAT;
                needsRedraw_ = true;
                Logger::Info("TAB: GAME_MODE → CHAT");
            }
            break;
            
        case InputEvent::KEY_ENTER:
            if (inputMode_ != NONE) {
                ProcessCompletedInput();
                inputBuffer_.clear();
                needsRedraw_ = true;
            }
            break;
            
        case InputEvent::KEY_BACKSPACE:
            if (inputMode_ != NONE && !inputBuffer_.empty()) {
                // UTF-8 한 글자 단위로 삭제
                size_t len = inputBuffer_.length();
                if (len > 0) {
                    size_t pos = len - 1;
                    while (pos > 0 && (static_cast<unsigned char>(inputBuffer_[pos]) & 0xC0) == 0x80) {
                        pos--;
                    }
                    inputBuffer_.erase(pos);
                }
                inputEchoNeedsUpdate_ = true;
            }
            break;
            
        case InputEvent::KEY_CHAR:
            if (inputMode_ != NONE) {
                // Wide character를 UTF-8로 변환하여 버퍼에 추가
                wchar_t wch = event.wch;
                if (wch >= 32 && wch < 0x110000) {  // 인쇄 가능한 문자
                    char utf8[5] = {0};
                    if (wch < 0x80) {
                        // ASCII (1바이트)
                        utf8[0] = static_cast<char>(wch);
                        inputBuffer_ += utf8;
                    } else if (wch < 0x800) {
                        // 2바이트
                        utf8[0] = static_cast<char>(0xC0 | (wch >> 6));
                        utf8[1] = static_cast<char>(0x80 | (wch & 0x3F));
                        inputBuffer_ += utf8;
                    } else if (wch < 0x10000) {
                        // 3바이트 (한글 등)
                        utf8[0] = static_cast<char>(0xE0 | (wch >> 12));
                        utf8[1] = static_cast<char>(0x80 | ((wch >> 6) & 0x3F));
                        utf8[2] = static_cast<char>(0x80 | (wch & 0x3F));
                        inputBuffer_ += utf8;
                    } else if (wch < 0x110000) {
                        // 4바이트 (이모지 등) - wchar_t는 16비트이므로 실제로는 도달 불가
                        unsigned long codepoint = static_cast<unsigned long>(wch);
                        utf8[0] = static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
                        utf8[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        utf8[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        utf8[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
                        inputBuffer_ += utf8;
                    }
                    inputEchoNeedsUpdate_ = true;
                }
            }
            break;
    }
}

// ===== 입력 완료 처리 (엔터 눌렀을 때) =====
void GameScreen::ProcessCompletedInput() {
    if (inputBuffer_.empty()) return;
    
    if (inputMode_ == INPUT_HINT_WORD) {
        // 힌트 단어 입력 완료
        hintWord_ = inputBuffer_;
        inputMode_ = INPUT_HINT_COUNT;
        Logger::Info("Hint word entered: " + hintWord_);
    } else if (inputMode_ == INPUT_HINT_COUNT) {
        // 힌트 숫자 입력 완료
        try {
            int count = std::stoi(inputBuffer_);
            if (count > 0) {
                ProvideHint(hintWord_, count);
                Logger::Info("Hint sent: " + hintWord_ + " (" + std::to_string(count) + ")");
                // 입력 모드 종료
                inputMode_ = NONE;
                hintWord_.clear();
            }
        } catch (...) {
            Logger::Info("Invalid hint count");
        }
    } else if (inputMode_ == INPUT_ANSWER) {
        // 답변 입력 완료 - 카드 단어 매칭
        std::string answer = inputBuffer_;
        
        // 입력된 단어와 일치하는 카드 찾기
        bool found = false;
        for (const auto& card : gameState_->cards) {
            if (!card.isRevealed && card.word == answer) {
                // 매칭 성공! 서버에 전송
                std::string cmd = std::string(PKT_ANSWER) + "|" + answer;
                client_->SendData(cmd);
                Logger::Info("Answer sent (matched card): " + answer);
                found = true;
                break;
            }
        }
        
        if (!found) {
            Logger::Info("Answer not matched: " + answer);
        }
        
        // 입력 버퍼만 클리어 (ANSWER 모드는 유지)
    } else if (inputMode_ == INPUT_CHAT) {
        // 채팅 전송
        SendChatMessage(inputBuffer_);
        Logger::Info("Chat sent: " + inputBuffer_);
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

void GameScreen::SendChatMessage(const std::string& message) {
    std::string cmd = std::string(PKT_CHAT) + "|" + message;
    client_->SendData(cmd);
}

int GameScreen::GetCardGridStartX() const {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    return 40;  // 플레이어 목록 오른쪽에 배치
}

int GameScreen::GetCardGridStartY() const {
    return 3;   // 힌트 아래에 시작
}

int GameScreen::GetChatPanelStartX() const {
    return 2;
}

int GameScreen::GetChatPanelStartY() const {
    return 14;
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
    }
    
    Logger::Info("[DEBUG] Phase changed to: " + std::to_string(static_cast<int>(newPhase)));
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
        Logger::Info("[DEBUG] Score updated - Red: " + std::to_string(redScore) + ", Blue: " + std::to_string(blueScore));
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

// ==================== 패킷 처리 ====================

void GameScreen::DrainPacketQueue() {
    // 패킷 큐에서 모든 대기 중인 패킷을 꺼내서 처리
    // (네트워크 스레드가 큐에 추가한 패킷들을 GUI 스레드에서 처리)
    if (!packetHandler_) return;  // 패킷 핸들러가 없으면 스킵
    
    while (true) {
        std::string pkt;
        {
            std::lock_guard<std::mutex> lock(g_packetQueueMutex);
            if (g_packetQueue.empty()) break;
            pkt = std::move(g_packetQueue.front());
            g_packetQueue.pop_front();
        }
        if (!pkt.empty()) {
            packetHandler_->ProcessPacket(pkt);
        }
    }
}

// ==================== 입력 모드 관리 ====================

void GameScreen::UpdateInputModeBasedOnGameState() {
    // 디버깅: 현재 GameState 값 출력
    Logger::Info(std::string("=== UpdateInputModeBasedOnGameState ==="));
    Logger::Info(std::string("  inGameStep: ") + std::to_string(gameState_->inGameStep));
    Logger::Info(std::string("  isMyLeader: ") + (gameState_->isMyLeader ? "true" : "false"));
    Logger::Info(std::string("  myTeam: ") + std::to_string(gameState_->myTeam));
    Logger::Info(std::string("  currentTurn: ") + std::to_string(gameState_->currentTurn));
    Logger::Info(std::string("  myPlayerIndex: ") + std::to_string(gameState_->myPlayerIndex));
    
    // 입력 모드 자동 설정
    if (gameState_->inGameStep == 0 && gameState_->isMyLeader && gameState_->myTeam == gameState_->currentTurn) {
        // HINT Phase && 내가 리더 && 내 팀 턴: 힌트 입력 모드
        inputMode_ = INPUT_HINT_WORD;
        Logger::Info("[INPUT MODE] Auto-entered HINT input mode (leader turn)");
    } else if (gameState_->inGameStep == 1 && !gameState_->isMyLeader && gameState_->myTeam == gameState_->currentTurn) {
        // ANSWER Phase && 내가 팀원 && 내 팀 턴: 답변 입력 모드
        inputMode_ = INPUT_ANSWER;
        Logger::Info("[INPUT MODE] Auto-entered ANSWER input mode (team member turn)");
    } else {
        // 그 외: 입력 모드 해제 (턴이 아니면 잠금)
        inputMode_ = NONE;
        Logger::Info("[INPUT MODE] Input mode disabled (not my turn)");
    }
    needsRedraw_ = true;
}
