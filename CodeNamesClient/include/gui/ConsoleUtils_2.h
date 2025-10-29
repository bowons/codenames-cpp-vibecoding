#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <windows.h>

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

class ConsoleUtils {
public:
    static void Initialize();
    static void Cleanup();
    static void Clear();
    static void SetCursorPosition(int x, int y);
    static void SetTextColor(ConsoleColor foreground, ConsoleColor background = ConsoleColor::BLACK);
    static void ResetTextColor();
    static void GetConsoleSize(int& width, int& height);
    static void PrintCentered(int y, const std::string& text, ConsoleColor color = ConsoleColor::WHITE);
    static void DrawBorder();
    static std::string GetInput(int maxLength, bool mask = false);
    static int GetKeyInput();
    static void PrintAt(int x, int y, const std::string& text);
    static void PrintColoredAt(int x, int y, const std::string& text, ConsoleColor color);

private:
    static HANDLE consoleHandle_;
    static CONSOLE_SCREEN_BUFFER_INFO consoleInfo_;
};