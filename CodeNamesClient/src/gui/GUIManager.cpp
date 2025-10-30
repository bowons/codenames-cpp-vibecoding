#include "../../include/gui/GUIManager.h"
#include <iostream>
#include <conio.h>
#include "../../include/core/Logger.h"
#include <mutex>
#include <deque>
#include <chrono>
#include <thread>
#include <functional>
#include <algorithm>
#include <string>
#include "../../include/core/PacketHandler.h"
#include "../../include/core/PacketProtocol.h"

// Externs: defined in main.cpp
extern std::mutex g_packetQueueMutex;
extern std::deque<std::string> g_packetQueue;
extern std::shared_ptr<PacketHandler> g_packetHandler;
extern std::mutex g_mainTasksMutex;
extern std::deque<std::function<void()>> g_mainTasks;

GUIManager::GUIManager(std::shared_ptr<GameState> gameState)
    : gameState_(gameState),
      client_(nullptr),
      currentScene_(SceneState::LOGIN),
      nextScene_(SceneState::LOGIN) {
    
    loginScreen_ = std::make_shared<LoginScreen>(gameState_);
    signupScreen_ = std::make_shared<SignupScreen>(gameState_, client_);
    mainScreen_ = std::make_shared<MainScreen>(gameState_, client_);
    gameScreen_ = std::make_shared<GameScreen>(gameState_, client_);
    resultScreen_ = std::make_shared<ResultScreen>(gameState_);
    // Register GameScreen as observer on main thread (avoid shared_from_this in ctor)
    if (gameScreen_) {
        gameState_->AddObserver(gameScreen_);
    }
}

GUIManager::~GUIManager() {
    // Unregister observer to avoid dangling pointers
    if (gameScreen_) {
        gameState_->RemoveObserver(gameScreen_);
    }
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
        // First, run any enqueued main-thread tasks (e.g., connection/disconnection handlers)
        while (true) {
            std::function<void()> task;
            {
                std::lock_guard<std::mutex> lock(g_mainTasksMutex);
                if (g_mainTasks.empty()) break;
                task = std::move(g_mainTasks.front());
                g_mainTasks.pop_front();
            }
            if (task) task();
        }

        // Then process pending network packets on the GUI/main thread before handling scenes.
        while (true) {
            std::string pkt;
            {
                std::lock_guard<std::mutex> lock(g_packetQueueMutex);
                if (g_packetQueue.empty()) break;
                pkt = std::move(g_packetQueue.front());
                g_packetQueue.pop_front();
            }
            if (g_packetHandler) {
                g_packetHandler->ProcessPacket(pkt);
            }
        }
        switch (currentScene_) {
            case SceneState::LOGIN:
                LoginSceneLoop();
                break;
            case SceneState::SIGNUP:
                SignupSceneLoop();
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

void GUIManager::SetNetworkClient(std::shared_ptr<IOCPClient> client) {
    client_ = client;
    // Propagate to screens in case they were constructed with a null client
    if (mainScreen_) mainScreen_->SetClient(client_);
    if (gameScreen_) gameScreen_->SetClient(client_);
}

void GUIManager::LoginSceneLoop() {
    LoginResult result = loginScreen_->Show();
    
    if (result == LoginResult::SUCCESS) {
        // 서버에 로그인 정보 전송 및 서버 응답(토큰) 대기
        gameState_->username = loginScreen_->GetUsername();

        if (client_) {
            // 전송: LOGIN|id|pw
            std::string cmd = std::string(PKT_LOGIN) + "|" + loginScreen_->GetUsername() + "|" + loginScreen_->GetPassword();
            // Log outbound LOGIN timestamp for diagnosis
            Logger::Info(std::string("Network TX: ") + cmd);
            client_->SendData(cmd);

            // 짧은 타임아웃 동안(예: 5초) 서버의 LOGIN_OK 또는 TOKEN_VALID 응답을 기다리며
            // GUI 스레드에서 패킷 큐를 소모하여 PacketHandler가 동작하도록 함.
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            bool gotTokenOrPhase = false;
            while (std::chrono::steady_clock::now() < deadline) {
                // Drain any pending network packets so PacketHandler runs on GUI thread
                std::string pkt;
                {
                    std::lock_guard<std::mutex> lock(g_packetQueueMutex);
                    if (!g_packetQueue.empty()) {
                        pkt = std::move(g_packetQueue.front());
                        g_packetQueue.pop_front();
                    }
                }
                if (!pkt.empty() && g_packetHandler) {
                    g_packetHandler->ProcessPacket(pkt);
                }

                // 서버가 토큰을 설정했거나(로그인 성공) GameState가 LOBBY로 변경되었으면 성공
                if (!gameState_->token.empty() || gameState_->currentPhase == GamePhase::LOBBY) {
                    gotTokenOrPhase = true;
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (gotTokenOrPhase) {
                nextScene_ = SceneState::MAIN_MENU;
            } else {
                // 타임아웃 혹은 실패: 로그인 실패 처리
                nextScene_ = SceneState::LOGIN;
            }
        } else {
            // 네트워크 클라이언트가 없으면 기존 동작(로컬 토큰 대체)
            gameState_->token = loginScreen_->GetUsername();  // placeholder
            gameState_->SetPhase(GamePhase::LOBBY);
            nextScene_ = SceneState::MAIN_MENU;
        }
    } else if (result == LoginResult::SIGNUP) {
        // 회원가입 화면으로 전환
        nextScene_ = SceneState::SIGNUP;
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
        case MainMenuOption::QUIT:
            nextScene_ = SceneState::EXIT;
            break;
    }
    
    TransitionScene(nextScene_);
}

void GUIManager::SignupSceneLoop() {
    // ensure signupScreen has client reference
    if (signupScreen_) signupScreen_->SetClient(client_);

    SignupResult res = signupScreen_->Show();

    if (res == SignupResult::SUCCESS) {
        // on success, PacketHandler may have set token/phase; go to main menu
        nextScene_ = SceneState::MAIN_MENU;
    } else if (res == SignupResult::BACK) {
        nextScene_ = SceneState::LOGIN;
    } else {
        // on error or duplicate, return to login for retry
        nextScene_ = SceneState::LOGIN;
    }

    TransitionScene(nextScene_);
}

void GUIManager::MatchingSceneLoop() {
    // 매칭 화면 표시
    ConsoleUtils::Clear();
    // draw a clean border and message (avoid leftover colored bars)
    ConsoleUtils::DrawBorder();
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintCentered(10, "Waiting for players...", ConsoleColor::CYAN);
    // send a queue/join request to server (if network client available)
    if (client_ && !gameState_->token.empty()) {
        std::string cmd = std::string(PKT_CMD_QUERY_WAIT) + "|" + gameState_->token;
        Logger::Info(std::string("Network TX: ") + cmd);
        client_->SendData(cmd);
    }
    // flush any pending key inputs so stray arrow keys don't affect later screens
    while (_kbhit()) _getch();
    
    // 게임 상태가 PLAYING으로 변경될 때까지 대기
    // During this loop we must also process network packets so WAIT_REPLY and GAME_START
    // are handled promptly on the GUI thread.
    while (gameState_->currentPhase != GamePhase::PLAYING) {
        // Drain pending network packets so PacketHandler runs on GUI thread
        while (true) {
            std::string pkt;
            {
                std::lock_guard<std::mutex> lock(g_packetQueueMutex);
                if (g_packetQueue.empty()) break;
                pkt = std::move(g_packetQueue.front());
                g_packetQueue.pop_front();
            }
            if (!pkt.empty() && g_packetHandler) {
                g_packetHandler->ProcessPacket(pkt);
            }
        }

        // Render minimal live progress: players waiting / max
        int count = gameState_->matchingCount;
        int maxPlayers = gameState_->matchingMax > 0 ? gameState_->matchingMax : 6; // fallback

    // redraw a small area with the live numbers
    std::string status = "Players: " + std::to_string(count) + " / " + std::to_string(maxPlayers);
    ConsoleUtils::PrintAt(10, 12, status);

    // simple progress bar
    int barWidth = 30;
    int filled = 0;
    if (maxPlayers > 0) filled = (count * barWidth) / maxPlayers;
    std::string bar = "[" + std::string(filled, '#') + std::string(barWidth - filled, ' ') + "]";
    ConsoleUtils::PrintAt(10, 14, bar);

        // 논블로킹 키 입력 처리 (ESC만 취소)
        if (_kbhit()) {
            int key = _getch();
            if (key == 27) {  // ESC
                nextScene_ = SceneState::MAIN_MENU;
                TransitionScene(nextScene_);
                return;
            }
        }

        Sleep(100);
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
