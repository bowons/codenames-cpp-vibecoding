#pragma once

#include <string>
#include <memory>
#include "../core/GameState.h"
#include "../core/IOCPClient.h"

enum class MainMenuOption {
    START_GAME,
    PROFILE,
    QUIT
};

class MainScreen {
public:
    MainScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client);
    
    // 메인 메뉴 화면 표시 및 선택 처리
    MainMenuOption Show();
    
    // 사용자 프로필 조회
    void DisplayProfile();

private:
    std::shared_ptr<GameState> gameState_;
    std::shared_ptr<IOCPClient> client_;
    MainMenuOption selectedOption_;
    int currentSelection_;
    
    // 메인 메뉴 화면 그리기
    void Draw();
    
    // 입력 처리
    void HandleInput(int key);
    
    // 프로필 정보 표시
    void DrawProfile();
};
