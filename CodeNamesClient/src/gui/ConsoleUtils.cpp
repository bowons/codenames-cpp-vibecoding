#include "../../include/gui/ConsoleUtils.h"
#include <iostream>
#include <deque>
#include <mutex>
#include <conio.h>
#include <iomanip>

// 정적 멤버 변수 초기화
HANDLE ConsoleUtils::consoleHandle_ = nullptr;
CONSOLE_SCREEN_BUFFER_INFO ConsoleUtils::consoleInfo_ = {};
static std::deque<std::string> topStatusLines_;
static std::mutex statusMutex_;
static const int MAX_TOP_STATUS_LINES = 3; // 화면 상단에 표시할 최대 줄 수

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

    // 상단 상태 로그들을 테두리 바로 아래부터 표시
    std::deque<std::string> linesCopy;
    {
        std::lock_guard<std::mutex> lk(statusMutex_);
        linesCopy = topStatusLines_;
    }
    int lineY = 1; // 테두리 바로 아래
    for (size_t i = 0; i < linesCopy.size() && lineY < height - 1; ++i, ++lineY) {
        std::string line = linesCopy[i];
        int maxInner = width - 2;
        if (static_cast<int>(line.length()) > maxInner) line = line.substr(0, maxInner);
        // 왼쪽(안쪽) 시작 위치
        int startX = 1;
        SetCursorPosition(startX, lineY);
        // 덮어쓰기: 길이 이후 공백으로 지워 잔상을 남기지 않음
        std::cout << line;
        for (int p = static_cast<int>(line.length()); p < maxInner; ++p) std::cout << ' ';
    }
    
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
    // Use wide-character input to support Unicode (Hangul) properly.
    std::wstring winput;
    while (true) {
        wchar_t wc = _getwch(); // wide-character get

        if (wc == L'\r' || wc == L'\n') {
            // newline - echo and break
            const wchar_t nl[] = L"\r\n";
            DWORD written = 0;
            WriteConsoleW(consoleHandle_, nl, 2, &written, NULL);
            break;
        } else if (wc == L'\b') {
            if (!winput.empty()) {
                // remove last wide character
                winput.pop_back();
                // move cursor back, overwrite with space, move back again
                const wchar_t bs[] = L"\b \b";
                DWORD written = 0;
                WriteConsoleW(consoleHandle_, bs, 3, &written, NULL);
            }
        } else {
            // accept printable characters; limit by maxLength (in characters)
            if ((int)winput.size() < maxLength) {
                winput.push_back(wc);
                DWORD written = 0;
                if (mask) {
                    const wchar_t star = L'*';
                    WriteConsoleW(consoleHandle_, &star, 1, &written, NULL);
                } else {
                    WriteConsoleW(consoleHandle_, &wc, 1, &written, NULL);
                }
            }
        }
    }

    // Convert wide string (UTF-16) to UTF-8
    if (winput.empty()) return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, winput.c_str(), (int)winput.size(), NULL, 0, NULL, NULL);
    std::string out;
    if (needed > 0) {
        out.resize(needed);
        WideCharToMultiByte(CP_UTF8, 0, winput.c_str(), (int)winput.size(), &out[0], needed, NULL, NULL);
    }
    return out;
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

void ConsoleUtils::SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lk(statusMutex_);
    // 빈 문자열이면 무시
    if (status.empty()) return;
    // 새 메시지를 큐에 추가하고 필요하면 오래된 항목 삭제
    topStatusLines_.push_back(status);
    while (static_cast<int>(topStatusLines_.size()) > MAX_TOP_STATUS_LINES) {
        topStatusLines_.pop_front();
    }
}

void ConsoleUtils::DrawInputBox(int x, int y, int width, const std::string& prompt) {
    if (!consoleHandle_) return;
    // Ensure prompt fits
    int maxInner = width;
    std::string display = prompt;
    if ((int)display.length() > maxInner) display = display.substr(0, maxInner);

    SetTextColor(ConsoleColor::CYAN);
    SetCursorPosition(x, y);
    std::cout << display;
    // fill the rest with spaces to create the box area
    for (int i = (int)display.length(); i < maxInner; ++i) std::cout << ' ';
    ResetTextColor();
    // Position cursor after prompt
    SetCursorPosition(x + (int)display.length(), y);
}

void ConsoleUtils::ClearLine(int x, int y, int width) {
    if (!consoleHandle_) return;
    SetCursorPosition(x, y);
    for (int i = 0; i < width; ++i) std::cout << ' ';
    SetCursorPosition(x, y);
}
