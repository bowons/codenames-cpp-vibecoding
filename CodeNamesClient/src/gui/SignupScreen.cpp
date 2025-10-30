#include "../../include/gui/SignupScreen.h"
#include "../../include/gui/ConsoleUtils.h"
#include "../../include/core/PacketProtocol.h"
#include "../../include/core/PacketHandler.h"
#include "../../include/core/Logger.h"
#include <iostream>
#include <conio.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <deque>

// extern packet handler to allow using global packet processing if needed
extern std::shared_ptr<PacketHandler> g_packetHandler;
// access to the global packet queue so modal screens can drain incoming packets
extern std::mutex g_packetQueueMutex;
extern std::deque<std::string> g_packetQueue;

SignupScreen::SignupScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client)
    : gameState_(gameState), client_(client), currentState_(SignupState::ID_INPUT), signupResult_(SignupResult::NONE) {
}

SignupResult SignupScreen::Show() {
    ConsoleUtils::Initialize();
    ConsoleUtils::Clear();
    // ensure transient result is reset for each Show() invocation
    signupResult_ = SignupResult::NONE;

    while (signupResult_ == SignupResult::NONE) {
        ConsoleUtils::Clear();
        Draw();
        ConsoleUtils::SetCursorPosition(0, 0);

        int key = _getch();
        HandleInput(key);
    }

    return signupResult_;
}

void SignupScreen::Draw() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);

    ConsoleUtils::DrawBorder();
    ConsoleUtils::PrintCentered(2, "=== CODE NAMES ===", ConsoleColor::YELLOW);
    ConsoleUtils::PrintCentered(3, "Signup", ConsoleColor::CYAN);

    int y = height / 3;
    int x = (width - 50) / 2;

    ConsoleUtils::PrintAt(x, y, "ID: ");
    if (currentState_ == SignupState::ID_INPUT) ConsoleUtils::SetTextColor(ConsoleColor::WHITE, ConsoleColor::BLACK);
    ConsoleUtils::PrintAt(x + 5, y, username_);
    ConsoleUtils::ResetTextColor();

    ConsoleUtils::PrintAt(x, y + 2, "PW: ");
    if (currentState_ == SignupState::PASSWORD_INPUT) ConsoleUtils::SetTextColor(ConsoleColor::WHITE, ConsoleColor::BLACK);
    ConsoleUtils::PrintAt(x + 5, y + 2, std::string(password_.length(), '*'));
    ConsoleUtils::ResetTextColor();

    ConsoleUtils::PrintAt(x, y + 4, "Nickname: ");
    if (currentState_ == SignupState::NICK_INPUT) ConsoleUtils::SetTextColor(ConsoleColor::WHITE, ConsoleColor::BLACK);
    ConsoleUtils::PrintAt(x + 10, y + 4, nickname_);
    ConsoleUtils::ResetTextColor();

    // Buttons
    ConsoleUtils::PrintAt(x, y + 6, "[");
    if (currentState_ == SignupState::SIGNUP_BUTTON) ConsoleUtils::SetTextColor(ConsoleColor::BLACK, ConsoleColor::WHITE);
    ConsoleUtils::PrintAt(x + 1, y + 6, "Signup");
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintAt(x + 7, y + 6, "]");

    ConsoleUtils::PrintAt(x + 10, y + 6, "[");
    if (currentState_ == SignupState::BACK_BUTTON) ConsoleUtils::SetTextColor(ConsoleColor::BLACK, ConsoleColor::WHITE);
    ConsoleUtils::PrintAt(x + 11, y + 6, "Back");
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintAt(x + 15, y + 6, "]");

    DrawStatusMessage();
}

void SignupScreen::HandleInput(int key) {
    if (key == 224 || key == 0) {
        key = _getch();
        switch (key) {
            case 72: // UP
            case 75: // LEFT
                switch (currentState_) {
                    case SignupState::PASSWORD_INPUT: currentState_ = SignupState::ID_INPUT; break;
                    case SignupState::NICK_INPUT: currentState_ = SignupState::PASSWORD_INPUT; break;
                    case SignupState::SIGNUP_BUTTON: currentState_ = SignupState::NICK_INPUT; break;
                    case SignupState::BACK_BUTTON: currentState_ = SignupState::SIGNUP_BUTTON; break;
                    default: break;
                }
                return;
            case 80: // DOWN
            case 77: // RIGHT
                switch (currentState_) {
                    case SignupState::ID_INPUT: currentState_ = SignupState::PASSWORD_INPUT; break;
                    case SignupState::PASSWORD_INPUT: currentState_ = SignupState::NICK_INPUT; break;
                    case SignupState::NICK_INPUT: currentState_ = SignupState::SIGNUP_BUTTON; break;
                    case SignupState::SIGNUP_BUTTON: currentState_ = SignupState::BACK_BUTTON; break;
                    default: break;
                }
                return;
        }
    }

    switch (key) {
        case 9: // TAB
            switch (currentState_) {
                case SignupState::ID_INPUT: currentState_ = SignupState::PASSWORD_INPUT; break;
                case SignupState::PASSWORD_INPUT: currentState_ = SignupState::NICK_INPUT; break;
                case SignupState::NICK_INPUT: currentState_ = SignupState::SIGNUP_BUTTON; break;
                case SignupState::SIGNUP_BUTTON: currentState_ = SignupState::BACK_BUTTON; break;
                case SignupState::BACK_BUTTON: currentState_ = SignupState::ID_INPUT; break;
            }
            break;
        case 13: // ENTER
            if (currentState_ == SignupState::SIGNUP_BUTTON) {
                // send signup packet
                if (username_.empty() || password_.empty() || nickname_.empty()) {
                    signupResult_ = SignupResult::FAILURE;
                } else {
                        if (client_) {
                        std::string pkt = std::string(PKT_SIGNUP) + "|" + username_ + "|" + password_ + "|" + nickname_;
                        // Log outbound packet timestamp for latency diagnosis
                        Logger::Info(std::string("Network TX: ") + pkt);
                        client_->SendData(pkt);

                        // Wait up to 5s for server response, but actively drain the global packet queue
                        // so PacketHandler runs on the GUI thread and updates GameState immediately.
                        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                        while (std::chrono::steady_clock::now() < deadline) {
                            // Drain pending packets that the network thread enqueued
                            std::string pktIn;
                            {
                                std::lock_guard<std::mutex> lock(g_packetQueueMutex);
                                if (!g_packetQueue.empty()) {
                                    pktIn = std::move(g_packetQueue.front());
                                    g_packetQueue.pop_front();
                                }
                            }
                            if (!pktIn.empty() && g_packetHandler) {
                                g_packetHandler->ProcessPacket(pktIn);
                            }

                            if (!gameState_->token.empty() || gameState_->currentPhase == GamePhase::LOBBY) {
                                signupResult_ = SignupResult::SUCCESS;
                                break;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        }
                        if (signupResult_ == SignupResult::NONE) {
                            signupResult_ = SignupResult::FAILURE;
                        }
                    } else {
                        // no client -> simulate success
                        signupResult_ = SignupResult::SUCCESS;
                    }

                }
            } else if (currentState_ == SignupState::BACK_BUTTON) {
                signupResult_ = SignupResult::BACK;
            }
            break;
        case 8: // BACKSPACE
            if (currentState_ == SignupState::ID_INPUT && !username_.empty()) username_.pop_back();
            else if (currentState_ == SignupState::PASSWORD_INPUT && !password_.empty()) password_.pop_back();
            else if (currentState_ == SignupState::NICK_INPUT && !nickname_.empty()) nickname_.pop_back();
            break;
        default:
            if (key >= 32 && key <= 126) {
                if (currentState_ == SignupState::ID_INPUT && username_.length() < 32) username_ += char(key);
                else if (currentState_ == SignupState::PASSWORD_INPUT && password_.length() < 32) password_ += char(key);
                else if (currentState_ == SignupState::NICK_INPUT && nickname_.length() < 32) nickname_ += char(key);
            }
            break;
    }
}

void SignupScreen::DrawStatusMessage() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);

    std::string message;
    ConsoleColor color = ConsoleColor::WHITE;

    switch (signupResult_) {
        case SignupResult::SUCCESS:
            message = "Signup successful!";
            color = ConsoleColor::GREEN;
            break;
        case SignupResult::DUPLICATE:
            message = "ID or nickname already exists.";
            color = ConsoleColor::RED;
            break;
        case SignupResult::FAILURE:
            message = "Signup error or missing fields.";
            color = ConsoleColor::RED;
            break;
        default:
            return;
    }

    ConsoleUtils::PrintCentered(height - 3, message, color);
}
