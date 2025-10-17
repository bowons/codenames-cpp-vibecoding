#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "IOCPServer.h"

// 전역 서버 포인터와 종료 이벤트
IOCPServer* g_server = nullptr;
HANDLE g_shutdownEvent = nullptr;

// Windows 콘솔 이벤트 핸들러
BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        std::cout << "\nServer shutting down..." << std::endl;
        if (g_server) {
            g_server->Stop();
        }
        // 이벤트 시그널로 메인 스레드 깨움
        SetEvent(g_shutdownEvent);
        return TRUE;
    default:
        return FALSE;
    }
}

int main() {
    std::cout << "CodeNames IOCP Server Starting..." << std::endl;
    
    // 종료 이벤트 생성
    g_shutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_shutdownEvent) {
        std::cerr << "Failed to create shutdown event" << std::endl;
        return -1;
    }
    
    // Windows 콘솔 핸들러 등록
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    IOCPServer server;
    g_server = &server;
    
    if (!server.Initialize()) {
        std::cerr << "Server initialization failed" << std::endl;
        system("pause");
        CloseHandle(g_shutdownEvent);
        return -1;
    }
    std::cout << "Server initialized on port " << IOCPServer::SERVER_PORT << std::endl;

    server.Start();
    std::cout << "Server started! Press Ctrl+C to stop." << std::endl;
    
    // 이벤트가 시그널될 때까지 블로킹
    WaitForSingleObject(g_shutdownEvent, INFINITE);
    
    std::cout << "Server stopped." << std::endl;
    CloseHandle(g_shutdownEvent);

    return 0;
}