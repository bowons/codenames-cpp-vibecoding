#include "../../include/gui/GUIManager.h"
#include <iostream>
#include <conio.h>

GUIManager::GUIManager()
    : gameState_(std::make_shared<GameState>()),
      client_(nullptr),
      currentScene_(SceneState::LOGIN),
      nextScene_(SceneState::LOGIN) {
    
    loginScreen_ = std::make_shared<LoginScreen>(gameState_);
    mainScreen_ = std::make_shared<MainScreen>(gameState_, client_);
    gameScreen_ = std::make_shared<GameScreen>(gameState_, client_);
    resultScreen_ = std::make_shared<ResultScreen>(gameState_);
}

GUIManager::~GUIManager() {
    ConsoleUtils::Cleanup();
}

bool GUIManager::Initialize() {
    ConsoleUtils::Initialize();
    return true;
}

void GUIManager::Run() {
    if (!Initialize()) {
        return;
    }
    
    while (currentScene_ != SceneState::EXIT) {
        switch (currentScene_) {
            case SceneState::LOGIN:
                LoginSceneLoop();
                break;
            case SceneState::MAIN_MENU:
                MainMenuSceneLoop();
                break;
            case SceneState::MATCHING:
                MatchingSceneLoop();
                break;
            case SceneState::GAME:
                GameSceneLoop();
                break;
            case SceneState::RESULT:
                ResultSceneLoop();
                break;
            default:
                currentScene_ = SceneState::EXIT;
                break;
        }
    }
    
    ConsoleUtils::Cleanup();
}

void GUIManager::TransitionScene(SceneState newScene) {
    currentScene_ = newScene;
}

void GUIManager::LoginSceneLoop() {
    LoginResult result = loginScreen_->Show();
    
    if (result == LoginResult::SUCCESS) {
        // 서버에 로그인 정보 전송
        gameState_->username = loginScreen_->GetUsername();
        gameState_->token = loginScreen_->GetUsername();  // 실제로는 서버에서 받은 토큰
        gameState_->SetPhase(GamePhase::LOBBY);
        
        nextScene_ = SceneState::MAIN_MENU;
    } else {
        // 로그인 실패 - 메인 메뉴로 복귀
        nextScene_ = SceneState::LOGIN;
    }
    
    TransitionScene(nextScene_);
}

void GUIManager::MainMenuSceneLoop() {
    MainMenuOption option = mainScreen_->Show();
    
    switch (option) {
        case MainMenuOption::START_GAME:
            gameState_->SetPhase(GamePhase::MATCHING);
            nextScene_ = SceneState::MATCHING;
            break;
        case MainMenuOption::PROFILE:
            mainScreen_->DisplayProfile();
            break;
        case MainMenuOption::QUIT:
            nextScene_ = SceneState::EXIT;
            break;
    }
    
    TransitionScene(nextScene_);
}

void GUIManager::MatchingSceneLoop() {
    // 매칭 화면 표시
    ConsoleUtils::Clear();
    ConsoleUtils::PrintCentered(10, "Waiting for players...", ConsoleColor::CYAN);
    
    // 게임 상태가 PLAYING으로 변경될 때까지 대기
    while (gameState_->currentPhase != GamePhase::PLAYING) {
        Sleep(100);
        
        // 논블로킹 키 입력 처리
        if (_kbhit()) {
            int key = _getch();
            if (key == 27) {  // ESC
                nextScene_ = SceneState::MAIN_MENU;
                TransitionScene(nextScene_);
                return;
            }
        }
    }
    
    nextScene_ = SceneState::GAME;
    TransitionScene(nextScene_);
}

void GUIManager::GameSceneLoop() {
    gameScreen_->Show();
    
    nextScene_ = SceneState::RESULT;
    TransitionScene(nextScene_);
}

void GUIManager::ResultSceneLoop() {
    resultScreen_->Show();
    
    nextScene_ = SceneState::MAIN_MENU;
    TransitionScene(nextScene_);
}
