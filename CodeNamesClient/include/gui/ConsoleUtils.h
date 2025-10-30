#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <windows.h>

// ==================== 색상 정의 ====================
enum class ConsoleColor {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    YELLOW = 6,
    WHITE = 7
};

// ==================== 콘솔 유틸리티 ====================
class ConsoleUtils {
public:
    // 콘솔 초기화
    static void Initialize();
    
    // 콘솔 정리
    static void Cleanup();
    
    // 화면 지우기
    static void Clear();
    
    // 커서 위치 이동
    static void SetCursorPosition(int x, int y);
    
    // 텍스트 색상 설정
    static void SetTextColor(ConsoleColor foreground, ConsoleColor background = ConsoleColor::BLACK);
    
    // 텍스트 색상 리셋
    static void ResetTextColor();
    
    // 콘솔 크기 조회
    static void GetConsoleSize(int& width, int& height);
    
    // 중앙 정렬된 문자열 출력
    static void PrintCentered(int y, const std::string& text, ConsoleColor color = ConsoleColor::WHITE);
    
    // 테두리 그리기
    static void DrawBorder();
    
    // 텍스트 입력 받기 (마스킹 옵션)
    static std::string GetInput(int maxLength, bool mask = false);
    
    // 키 입력 감지 (논블로킹)
    static int GetKeyInput();
    
    // 특정 위치에 텍스트 출력
    static void PrintAt(int x, int y, const std::string& text);
    
    // 특정 위치에 색상이 있는 텍스트 출력
    static void PrintColoredAt(int x, int y, const std::string& text, ConsoleColor color);

    // 상태 표시(상단 테두리에 한 줄로 표시)
    static void SetStatus(const std::string& status);

    // Draw a simple input box/prompt at (x,y) with given width and prompt text.
    // This does not read input; it only draws the prompt and reserves space.
    static void DrawInputBox(int x, int y, int width, const std::string& prompt);

    // Clear a portion of a line starting at (x,y) of given width (overwrite with spaces)
    static void ClearLine(int x, int y, int width);

private:
    static HANDLE consoleHandle_;
    static CONSOLE_SCREEN_BUFFER_INFO consoleInfo_;
};
