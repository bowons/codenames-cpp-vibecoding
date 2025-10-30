#pragma once

#include <memory>
#include <string>
#include <conio.h>
#include "../core/GameState.h"
#include "../core/IOCPClient.h"
#include "ConsoleUtils.h"
#include "LoginScreen.h"
#include "SignupScreen.h"
#include "MainScreen.h"
#include "GameScreen.h"
#include "ResultScreen.h"

enum class SceneState {
    LOGIN,
    SIGNUP,
    MAIN_MENU,
    MATCHING,
    GAME,
    RESULT,
    EXIT,
    ERROR_SCENE
};

class GUIManager {
public:
    GUIManager(std::shared_ptr<GameState> gameState);
    ~GUIManager();
    
    // GUI 시스템 초기화
    bool Initialize();
    
    // 메인 루프 (화면 전환 처리)
    void Run();
    
    // IOCPClient 연결
    void SetNetworkClient(std::shared_ptr<IOCPClient> client);
    
private:
    std::shared_ptr<GameState> gameState_;
    std::shared_ptr<IOCPClient> client_;
    
    std::shared_ptr<LoginScreen> loginScreen_;
    std::shared_ptr<SignupScreen> signupScreen_;
    std::shared_ptr<MainScreen> mainScreen_;
    std::shared_ptr<GameScreen> gameScreen_;
    std::shared_ptr<ResultScreen> resultScreen_;
    
    SceneState currentScene_;
    SceneState nextScene_;
    
    // 씬 전환 처리
    void TransitionScene(SceneState newScene);
    
    // 각 씬 루프
    void LoginSceneLoop();
    void SignupSceneLoop();
    void MainMenuSceneLoop();
    void MatchingSceneLoop();
    void GameSceneLoop();
    void ResultSceneLoop();
};
