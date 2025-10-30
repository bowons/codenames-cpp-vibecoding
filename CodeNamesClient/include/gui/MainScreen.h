#pragma once

#include <string>
#include <memory>
#include "../core/GameState.h"
#include "../core/IOCPClient.h"

enum class MainMenuOption {
    START_GAME,
    QUIT
};

class MainScreen {
public:
    MainScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client);
    
    // 메인 메뉴 화면 표시 및 선택 처리
    MainMenuOption Show();
    
    // 사용자 프로필 조회
    // (profile display removed)
    
    // 네트워크 클라이언트 설정(런타임에 변경 가능)
    void SetClient(std::shared_ptr<IOCPClient> client) { client_ = client; }

private:
    std::shared_ptr<GameState> gameState_;
    std::shared_ptr<IOCPClient> client_;
    MainMenuOption selectedOption_;
    int currentSelection_;
    
    // 메인 메뉴 화면 그리기
    void Draw();
    
    // 입력 처리
    // 반환값: 사용자가 항목을 확정(엔터 또는 숫자 입력)하면 true, 그렇지 않으면 false
    bool HandleInput(int key);
    
    // (profile display removed)
};
