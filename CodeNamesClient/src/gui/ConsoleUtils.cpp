#include "../../include/gui/ConsoleUtils.h"
#include <iostream>
#include <conio.h>
#include <iomanip>

// 정적 멤버 변수 초기화
HANDLE ConsoleUtils::consoleHandle_ = nullptr;
CONSOLE_SCREEN_BUFFER_INFO ConsoleUtils::consoleInfo_ = {};

void ConsoleUtils::Initialize() {
    consoleHandle_ = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(consoleHandle_, &consoleInfo_);
    
    // 콘솔 모드 설정
    SetConsoleMode(consoleHandle_, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void ConsoleUtils::Cleanup() {
    ResetTextColor();
    Clear();
}

void ConsoleUtils::Clear() {
    if (!consoleHandle_) return;
    
    COORD coord = {0, 0};
    DWORD count;
    GetConsoleScreenBufferInfo(consoleHandle_, &consoleInfo_);
    
    FillConsoleOutputCharacter(
        consoleHandle_,
        ' ',
        consoleInfo_.dwSize.X * consoleInfo_.dwSize.Y,
        coord,
        &count
    );
    
    SetCursorPosition(0, 0);
}

void ConsoleUtils::SetCursorPosition(int x, int y) {
    if (!consoleHandle_) return;
    
    COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
    SetConsoleCursorPosition(consoleHandle_, coord);
}

void ConsoleUtils::SetTextColor(ConsoleColor foreground, ConsoleColor background) {
    if (!consoleHandle_) return;
    
    WORD attributes = static_cast<WORD>(foreground) | (static_cast<WORD>(background) << 4);
    SetConsoleTextAttribute(consoleHandle_, attributes);
}

void ConsoleUtils::ResetTextColor() {
    SetTextColor(ConsoleColor::WHITE, ConsoleColor::BLACK);
}

void ConsoleUtils::GetConsoleSize(int& width, int& height) {
    if (!consoleHandle_) {
        width = 80;
        height = 25;
        return;
    }
    
    GetConsoleScreenBufferInfo(consoleHandle_, &consoleInfo_);
    width = consoleInfo_.dwSize.X;
    height = consoleInfo_.dwSize.Y;
}

void ConsoleUtils::PrintCentered(int y, const std::string& text, ConsoleColor color) {
    int width, height;
    GetConsoleSize(width, height);
    
    int x = (width - text.length()) / 2;
    PrintColoredAt(x, y, text, color);
}

void ConsoleUtils::DrawBorder() {
    int width, height;
    GetConsoleSize(width, height);
    
    SetTextColor(ConsoleColor::CYAN);
    
    // 위쪽 테두리
    SetCursorPosition(0, 0);
    std::cout << "┌";
    for (int i = 1; i < width - 1; i++) std::cout << "─";
    std::cout << "┐";
    
    // 양쪽 테두리
    for (int y = 1; y < height - 1; y++) {
        SetCursorPosition(0, y);
        std::cout << "│";
        SetCursorPosition(width - 1, y);
        std::cout << "│";
    }
    
    // 아래쪽 테두리
    SetCursorPosition(0, height - 1);
    std::cout << "└";
    for (int i = 1; i < width - 1; i++) std::cout << "─";
    std::cout << "┘";
    
    ResetTextColor();
}

std::string ConsoleUtils::GetInput(int maxLength, bool mask) {
    std::string input;
    int ch;
    
    while (true) {
        ch = _getch();
        
        if (ch == '\r' || ch == '\n') {
            std::cout << std::endl;
            break;
        } else if (ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";
            }
        } else if (ch >= 32 && ch <= 126 && input.length() < maxLength) {
            input += char(ch);
            std::cout << (mask ? '*' : char(ch));
        }
    }
    
    return input;
}

int ConsoleUtils::GetKeyInput() {
    if (_kbhit()) {
        return _getch();
    }
    return -1;
}

void ConsoleUtils::PrintAt(int x, int y, const std::string& text) {
    SetCursorPosition(x, y);
    std::cout << text;
}

void ConsoleUtils::PrintColoredAt(int x, int y, const std::string& text, ConsoleColor color) {
    SetTextColor(color);
    PrintAt(x, y, text);
    ResetTextColor();
}
