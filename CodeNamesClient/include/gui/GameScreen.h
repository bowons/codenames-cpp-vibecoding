#pragma once

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include "../core/GameState.h"
#include "../core/IOCPClient.h"

// 입력 모드
enum InputMode {
    NONE,
    INPUT_HINT_WORD,    // 힌트 단어 입력
    INPUT_HINT_COUNT,   // 힌트 숫자 입력
    INPUT_ANSWER,       // 답변 입력
    INPUT_CHAT          // 채팅 입력
};

// 입력 이벤트 (입력 스레드 → 메인 스레드)
struct InputEvent {
    enum Type {
        KEY_CHAR,       // 일반 문자 입력
        KEY_BACKSPACE,  // 백스페이스
        KEY_ENTER,      // 엔터
        KEY_ESC,        // ESC
        KEY_TAB         // TAB
    };
    
    Type type;
    wchar_t wch;  // 문자 (KEY_CHAR일 때만 사용)
    
    InputEvent(Type t, wchar_t c = 0) : type(t), wch(c) {}
};

class PacketHandler;  // forward declaration

class GameScreen : public GameStateObserver, public std::enable_shared_from_this<GameScreen> {
public:
    GameScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client);
    ~GameScreen();
    
    // 게임 화면 표시 및 게임 루프
    void Show();
    
    // Allow runtime injection of the network client (used by GUIManager)
    void SetClient(std::shared_ptr<IOCPClient> client) { client_ = client; }
    
    // Allow runtime injection of packet handler (used by GUIManager)
    void SetPacketHandler(std::shared_ptr<PacketHandler> handler) { packetHandler_ = handler; }
    
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
    std::shared_ptr<PacketHandler> packetHandler_;  // 패킷 처리기 (런타임 주입)
    
    std::atomic<bool> gameRunning_;
    bool needsRedraw_;           // 재그리기 필요 여부
    int selectedCardIndex_;
    std::string inputBuffer_;    // UTF-8 입력 버퍼 (메인 스레드에서만 접근)
    int lastKnownRedScore_;      // 이전 점수 추적
    int lastKnownBlueScore_;
    int lastMessageCount_;       // 이전 메시지 개수 추적
    
    // ===== 스레드 관련 =====
    std::thread inputThread_;             // 입력 전담 스레드
    std::mutex inputEventMutex_;          // 입력 이벤트 큐 보호
    std::deque<InputEvent> inputEvents_;  // 입력 이벤트 큐 (입력 스레드 → 메인 스레드)
    std::atomic<bool> inputThreadRunning_;
    
    // 입력 스레드 함수
    void InputThreadFunc();
    
    // 입력 이벤트 처리 (메인 스레드)
    void ProcessInputEvents();
    void HandleInputEvent(const InputEvent& event);
    
    // 패킷 큐 드레인 (메인 루프에서 호출)
    void DrainPacketQueue();
    
    // 입력 모드 관련
    InputMode inputMode_;        // 현재 입력 모드 (NONE이면 입력 불가)
    std::string hintWord_;       // 힌트 단어 임시 저장
    std::atomic<bool> inputEchoNeedsUpdate_;  // 입력 에코 갱신 필요 여부 (스레드 안전)

    // 게임 화면 그리기
    void DrawGameBoard();
    void DrawPlayerInfo();
    void DrawScoreBoard();
    void DrawCardGrid();
    void DrawChatPanel();
    void DrawHintPanel();
    void DrawStatusBar();
    
    // 입력 처리
    void ProcessCompletedInput();  // 입력 완료 처리 (엔터 눌렀을 때)
    void SelectCard(int cardIndex);
    void ProvideHint(const std::string& word, int count);
    void SendChatMessage(const std::string& message);  // Windows API 충돌 방지
    
    // 입력 모드 관리
    void UpdateInputModeBasedOnGameState();
    
    // 레이아웃 계산
    int GetCardGridStartX() const;
    int GetCardGridStartY() const;
    int GetChatPanelStartX() const;
    int GetChatPanelStartY() const;
};
