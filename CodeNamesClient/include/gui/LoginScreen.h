#pragma once

#include <string>
#include <memory>
#include "../core/GameState.h"

enum class LoginState {
    ID_INPUT,
    PASSWORD_INPUT,
    LOGIN_BUTTON,
    SIGNUP_BUTTON
};

enum class LoginResult {
    NONE = -1,
    SUCCESS = 0,
    NO_ACCOUNT = 1,
    WRONG_PASSWORD = 2,
    SUSPENDED = 3,
    ERROR_LOGIN = 4
};

class LoginScreen {
public:
    LoginScreen(std::shared_ptr<GameState> gameState);
    
    // 로그인 화면 표시 및 입력 처리
    LoginResult Show();
    
    // 입력한 ID와 비밀번호 반환
    std::string GetUsername() const { return username_; }
    std::string GetPassword() const { return password_; }

private:
    std::shared_ptr<GameState> gameState_;
    std::string username_;
    std::string password_;
    LoginState currentState_;
    LoginResult loginResult_;
    
    // 로그인 화면 그리기
    void Draw();
    
    // 입력 처리
    void HandleInput(int key);
    
    // 위치 계산
    void UpdatePositions();
    
    // 로그인 상태 표시
    void DrawStatusMessage();
};
