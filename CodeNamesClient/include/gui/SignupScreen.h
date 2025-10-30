#pragma once

#include <string>
#include <memory>
#include "../core/GameState.h"
#include "../core/IOCPClient.h"

enum class SignupState {
    ID_INPUT,
    PASSWORD_INPUT,
    NICK_INPUT,
    SIGNUP_BUTTON,
    BACK_BUTTON
};

enum class SignupResult {
    NONE = -1,
    SUCCESS = 0,
    DUPLICATE = 1,
    FAILURE = 2,
    BACK = 3
};

class SignupScreen {
public:
    SignupScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client = nullptr);

    SignupResult Show();

    void SetClient(std::shared_ptr<IOCPClient> client) { client_ = client; }

    std::string GetUsername() const { return username_; }
    std::string GetPassword() const { return password_; }
    std::string GetNickname() const { return nickname_; }

private:
    std::shared_ptr<GameState> gameState_;
    std::shared_ptr<IOCPClient> client_;

    std::string username_;
    std::string password_;
    std::string nickname_;
    SignupState currentState_;
    SignupResult signupResult_;

    void Draw();
    void HandleInput(int key);
    void DrawStatusMessage();
};
