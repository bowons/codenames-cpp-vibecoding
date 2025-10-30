#include "../../include/gui/LoginScreen.h"
#include "../../include/gui/ConsoleUtils.h"
#include <iostream>
#include <conio.h>

LoginScreen::LoginScreen(std::shared_ptr<GameState> gameState)
    : gameState_(gameState),
      currentState_(LoginState::ID_INPUT),
      loginResult_(LoginResult::NONE) {
}

LoginResult LoginScreen::Show() {
    ConsoleUtils::Initialize();
    ConsoleUtils::Clear();
    // Reset transient result so repeated calls to Show() require user action again
    loginResult_ = LoginResult::NONE;

    while (loginResult_ == LoginResult::NONE) {
        ConsoleUtils::Clear();
        Draw();
        ConsoleUtils::SetCursorPosition(0, 0);
        
        int key = _getch();
        HandleInput(key);
    }
    
    return loginResult_;
}

void LoginScreen::Draw() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    ConsoleUtils::DrawBorder();
    
    // 제목
    ConsoleUtils::PrintCentered(2, "=== CODE NAMES ===", ConsoleColor::YELLOW);
    ConsoleUtils::PrintCentered(3, "Login", ConsoleColor::CYAN);
    
    // ID 입력
    int y = height / 3;
    int x = (width - 40) / 2;
    
    ConsoleUtils::PrintAt(x, y, "ID: ");
    if (currentState_ == LoginState::ID_INPUT) {
        ConsoleUtils::SetTextColor(ConsoleColor::WHITE, ConsoleColor::BLACK);
    }
    ConsoleUtils::PrintAt(x + 5, y, username_);
    ConsoleUtils::ResetTextColor();
    
    // Password 입력
    ConsoleUtils::PrintAt(x, y + 2, "PW: ");
    if (currentState_ == LoginState::PASSWORD_INPUT) {
        ConsoleUtils::SetTextColor(ConsoleColor::WHITE, ConsoleColor::BLACK);
    }
    ConsoleUtils::PrintAt(x + 5, y + 2, std::string(password_.length(), '*'));
    ConsoleUtils::ResetTextColor();
    
    // 버튼
    ConsoleUtils::PrintAt(x, y + 4, "[");
    if (currentState_ == LoginState::LOGIN_BUTTON) {
        ConsoleUtils::SetTextColor(ConsoleColor::BLACK, ConsoleColor::WHITE);
    }
    ConsoleUtils::PrintAt(x + 1, y + 4, "Login");
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintAt(x + 6, y + 4, "]  ");
    
    ConsoleUtils::PrintAt(x + 10, y + 4, "[");
    if (currentState_ == LoginState::SIGNUP_BUTTON) {
        ConsoleUtils::SetTextColor(ConsoleColor::BLACK, ConsoleColor::WHITE);
    }
    ConsoleUtils::PrintAt(x + 11, y + 4, "Signup");
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintAt(x + 17, y + 4, "]");
    
    // 상태 메시지
    DrawStatusMessage();
}

void LoginScreen::HandleInput(int key) {
    if (key == 224 || key == 0) {  // 화살표 키의 첫 번째 바이트
        key = _getch();  // 두 번째 바이트 읽기
        switch (key) {
            case 72:  // 위 화살표
            case 75:  // 왼쪽 화살표
                switch (currentState_) {
                    case LoginState::PASSWORD_INPUT:
                        currentState_ = LoginState::ID_INPUT;
                        break;
                    case LoginState::LOGIN_BUTTON:
                        currentState_ = LoginState::PASSWORD_INPUT;
                        break;
                    case LoginState::SIGNUP_BUTTON:
                        currentState_ = LoginState::LOGIN_BUTTON;
                        break;
                    default:
                        break;
                }
                return;
            case 80:  // 아래 화살표
            case 77:  // 오른쪽 화살표
                switch (currentState_) {
                    case LoginState::ID_INPUT:
                        currentState_ = LoginState::PASSWORD_INPUT;
                        break;
                    case LoginState::PASSWORD_INPUT:
                        currentState_ = LoginState::LOGIN_BUTTON;
                        break;
                    case LoginState::LOGIN_BUTTON:
                        currentState_ = LoginState::SIGNUP_BUTTON;
                        break;
                    default:
                        break;
                }
                return;
        }
    }

    switch (key) {
        case 9:  // TAB - 상태 전환
            switch (currentState_) {
                case LoginState::ID_INPUT:
                    currentState_ = LoginState::PASSWORD_INPUT;
                    break;
                case LoginState::PASSWORD_INPUT:
                    currentState_ = LoginState::LOGIN_BUTTON;
                    break;
                case LoginState::LOGIN_BUTTON:
                    currentState_ = LoginState::SIGNUP_BUTTON;
                    break;
                case LoginState::SIGNUP_BUTTON:
                    currentState_ = LoginState::ID_INPUT;
                    break;
            }
            break;
            
        case 13:  // ENTER
            if (currentState_ == LoginState::LOGIN_BUTTON) {
                if (username_.empty() || password_.empty()) {
                    loginResult_ = LoginResult::ERROR_LOGIN;
                } else {
                    loginResult_ = LoginResult::SUCCESS;
                }
            } else if (currentState_ == LoginState::SIGNUP_BUTTON) {
                // 회원가입 화면으로 전환
                loginResult_ = LoginResult::SIGNUP;
            }
            break;
            
        case 8:  // BACKSPACE
            if (currentState_ == LoginState::ID_INPUT && !username_.empty()) {
                username_.pop_back();
            } else if (currentState_ == LoginState::PASSWORD_INPUT && !password_.empty()) {
                password_.pop_back();
            }
            break;
            
        default:  // 일반 문자 입력
            if (key >= 32 && key <= 126) {
                if (currentState_ == LoginState::ID_INPUT && username_.length() < 32) {
                    username_ += char(key);
                } else if (currentState_ == LoginState::PASSWORD_INPUT && password_.length() < 32) {
                    password_ += char(key);
                }
            }
            break;
    }
}

void LoginScreen::DrawStatusMessage() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    std::string message;
    ConsoleColor color = ConsoleColor::WHITE;
    
    switch (loginResult_) {
        case LoginResult::SUCCESS:
            message = "Login Successful!";
            color = ConsoleColor::GREEN;
            break;
        case LoginResult::NO_ACCOUNT:
            message = "Account not found.";
            color = ConsoleColor::RED;
            break;
        case LoginResult::WRONG_PASSWORD:
            message = "Wrong password.";
            color = ConsoleColor::RED;
            break;
        case LoginResult::SUSPENDED:
            message = "Account suspended.";
            color = ConsoleColor::RED;
            break;
        case LoginResult::ERROR_LOGIN:
            message = "Please fill in all fields.";
            color = ConsoleColor::RED;
            break;
        default:
            return;
    }
    
    ConsoleUtils::PrintCentered(height - 3, message, color);
}
