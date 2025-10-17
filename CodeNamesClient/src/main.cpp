#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include "../include/gui/GUIManager.h"
#include "../include/core/IOCPClient.h"
#include "../include/core/PacketHandler.h"
#include "../include/core/GameState.h"

// 전역 변수
std::shared_ptr<GUIManager> g_guiManager = nullptr;
std::shared_ptr<PacketHandler> g_packetHandler = nullptr;
std::shared_ptr<GameState> g_gameState = nullptr;

// 네트워크 데이터 수신 콜백
void OnNetworkDataReceived(const std::string& packetData) {
    if (g_packetHandler) {
        g_packetHandler->ProcessPacket(packetData);
    }
}

// 네트워크 연결 상태 콜백
void OnNetworkConnected() {
    std::cout << "[Network] Connected to server!" << std::endl;
    if (g_gameState) {
        g_gameState->SetPhase(GamePhase::LOBBY);
    }
}

// 네트워크 연결 해제 콜백
void OnNetworkDisconnected() {
    std::cout << "[Network] Disconnected from server!" << std::endl;
    if (g_gameState) {
        g_gameState->SetPhase(GamePhase::ERROR_PHASE);
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

        // 7️⃣ GUIManager 생성 (UI 관리)
        g_guiManager = std::make_unique<GUIManager>();
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
