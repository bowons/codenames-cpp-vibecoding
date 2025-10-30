#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <deque>
#include <functional>
#include "../include/gui/GUIManager.h"
#include "../include/core/IOCPClient.h"
#include "../include/core/PacketHandler.h"
#include "../include/core/GameState.h"
#include "../include/gui/ConsoleUtils.h"
#include "../include/core/Logger.h"

// 전역 변수
// Globals
std::shared_ptr<GUIManager> g_guiManager = nullptr;
std::shared_ptr<PacketHandler> g_packetHandler = nullptr;
std::shared_ptr<GameState> g_gameState = nullptr;

// Thread-safe packet queue: network thread pushes, GUI/main thread consumes
// 스레드-안전 패킷 큐: 네트워크 스레드가 푸시하고 GUI(메인) 스레드가 소비합니다.
std::mutex g_packetQueueMutex;
std::deque<std::string> g_packetQueue;

// Main-thread task queue for marshalling network callbacks to GUI thread
// 메인 스레드용 작업 큐: 네트워크 콜백을 GUI(메인) 스레드에서 안전하게 실행하도록 전달합니다.
std::mutex g_mainTasksMutex;
std::deque<std::function<void()>> g_mainTasks;

// 네트워크 데이터 수신 콜백
// 네트워크 스레드에서 수신된 패킷을 메인 스레드 큐로 옮겨 GUI 스레드가 처리하게 합니다.
void OnNetworkDataReceived(const std::string& packetData) {
    // Enqueue packet for processing on the main (GUI) thread to avoid
    // race conditions between network thread and UI thread.
    {
        std::lock_guard<std::mutex> lock(g_packetQueueMutex);
        g_packetQueue.push_back(packetData);
    }
}

// 네트워크 연결 상태 콜백
// 서버와의 연결이 성공했을 때 메인 스레드에서 GameState를 변경하도록 작업을 큐에 넣습니다.
void OnNetworkConnected() {
    std::cout << "[Network] Connected to server!" << std::endl;
    // Enqueue a main-thread task so GameState is modified on GUI thread
    {
        std::lock_guard<std::mutex> lock(g_mainTasksMutex);
        g_mainTasks.push_back([]() {
            if (g_gameState) {
                g_gameState->SetPhase(GamePhase::LOBBY);
            }
        });
    }
}

// 네트워크 연결 해제 콜백
// 서버와의 연결이 끊겼을 때 호출됩니다. 즉시 장면을 바꾸지 않고
// 메인 스레드에서 상태 표시만 업데이트하도록 작업을 큐에 넣습니다.
void OnNetworkDisconnected() {
    std::cout << "[Network] Disconnected from server!" << std::endl;

    // NOTE: avoid acquiring the log mutex in the network thread while also
    // trying to acquire g_mainTasksMutex — that ordering can deadlock if the
    // main thread holds g_mainTasksMutex and then logs (locks the log mutex).
    // To prevent this, perform logging on the main thread inside the enqueued
    // task instead of logging here.
    {
        std::lock_guard<std::mutex> lock(g_mainTasksMutex);
        g_mainTasks.push_back([]() {
            // Execute logging and status update on the GUI/main thread to
            // avoid lock-order inversion between logMutex and g_mainTasksMutex.
            Logger::Warn("[Network] Disconnected from server!");
            ConsoleUtils::SetStatus(std::string("Network: Disconnected from server!"));
        });
    }
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "=== CodeNames Client ===" << std::endl;
        std::cout << "Initializing..." << std::endl;

        // 1️⃣ GameState 생성 (모델)
        g_gameState = std::make_shared<GameState>();
        std::cout << "[GameState] Initialized" << std::endl;

        // 2️⃣ PacketHandler 생성 (패킷 처리)
        g_packetHandler = std::make_shared<PacketHandler>(g_gameState);
        std::cout << "[PacketHandler] Initialized with default handlers" << std::endl;

        // 3️⃣ IOCPClient 생성 (네트워크 클라이언트)
        auto client = std::make_shared<IOCPClient>();
        
        // 네트워크 콜백 등록
        client->onDataReceived = OnNetworkDataReceived;
        client->onConnected = OnNetworkConnected;
        client->onDisconnected = OnNetworkDisconnected;
        
        std::cout << "[IOCPClient] Callbacks registered" << std::endl;

        // 4️⃣ IOCPClient 초기화
        if (!client->Initialize()) {
            std::cerr << "[Error] Failed to initialize IOCPClient" << std::endl;
            return -1;
        }
        std::cout << "[IOCPClient] Initialized" << std::endl;

        // 5️⃣ 서버에 연결
        std::string serverAddr = "127.0.0.1";
        int serverPort = 55014;  // C++ IOCP 서버 포트
        
        if (argc >= 3) {
            serverAddr = argv[1];
            serverPort = std::stoi(argv[2]);
        }
        
        std::cout << "[IOCPClient] Connecting to " << serverAddr 
                  << ":" << serverPort << "..." << std::endl;
        
        if (!client->Connect(serverAddr, serverPort)) {
            std::cerr << "[Error] Failed to connect to server" << std::endl;
            return -1;
        }
        std::cout << "[IOCPClient] Connected!" << std::endl;

        // 6️⃣ 워커 스레드는 Connect 후 내부적으로 자동 시작됨
        std::cout << "[Network] Worker thread started" << std::endl;
        
        // 연결 안정화를 위해 잠깐 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 7️⃣ GUIManager 생성 (UI 관리) - 공유된 GameState를 전달
    g_guiManager = std::make_unique<GUIManager>(g_gameState);
        g_guiManager->SetNetworkClient(client);
        std::cout << "[GUIManager] Initialized" << std::endl;

        // 8️⃣ 메인 루프 시작
        std::cout << "[Main] Starting GUI main loop..." << std::endl;
        g_guiManager->Run();

        std::cout << "[Main] GUI loop finished, shutting down..." << std::endl;

        // 9️⃣ 정리
        client->Disconnect();
        std::cout << "[Main] Shutdown complete" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return -1;
    }
}
