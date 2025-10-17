#include "../../include/gui/MainScreen.h"
#include "../../include/gui/ConsoleUtils.h"
#include <iostream>
#include <conio.h>
#include <iomanip>

MainScreen::MainScreen(std::shared_ptr<GameState> gameState, std::shared_ptr<IOCPClient> client)
    : gameState_(gameState),
      client_(client),
      selectedOption_(MainMenuOption::START_GAME),
      currentSelection_(0) {
}

MainMenuOption MainScreen::Show() {
    ConsoleUtils::Initialize();
    ConsoleUtils::Clear();
    
    while (true) {
        ConsoleUtils::Clear();
        Draw();
        
        int key = _getch();
        HandleInput(key);
        
        if (selectedOption_ != MainMenuOption::QUIT) {
            return selectedOption_;
        }
    }
    
    return MainMenuOption::QUIT;
}

void MainScreen::Draw() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    ConsoleUtils::DrawBorder();
    
    // 제목
    ConsoleUtils::PrintCentered(2, "=== CODE NAMES ===", ConsoleColor::YELLOW);
    ConsoleUtils::PrintCentered(3, "Main Menu", ConsoleColor::CYAN);
    
    // 사용자 정보
    int y = height / 3;
    int x = (width - 40) / 2;
    
    ConsoleUtils::PrintAt(x, y, "Welcome, " + gameState_->username + "!");
    ConsoleUtils::PrintAt(x, y + 1, "Wins: " + std::to_string(0) + "  Losses: " + std::to_string(0));
    
    // 메뉴 옵션
    y += 4;
    
    // START GAME
    ConsoleUtils::PrintAt(x, y, "[");
    if (currentSelection_ == 0) {
        ConsoleUtils::SetTextColor(ConsoleColor::BLACK, ConsoleColor::GREEN);
    }
    ConsoleUtils::PrintAt(x + 1, y, "1. Start Game");
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintAt(x + 14, y, "]");
    
    // PROFILE
    ConsoleUtils::PrintAt(x, y + 2, "[");
    if (currentSelection_ == 1) {
        ConsoleUtils::SetTextColor(ConsoleColor::BLACK, ConsoleColor::GREEN);
    }
    ConsoleUtils::PrintAt(x + 1, y + 2, "2. Profile");
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintAt(x + 11, y + 2, "]");
    
    // QUIT
    ConsoleUtils::PrintAt(x, y + 4, "[");
    if (currentSelection_ == 2) {
        ConsoleUtils::SetTextColor(ConsoleColor::BLACK, ConsoleColor::RED);
    }
    ConsoleUtils::PrintAt(x + 1, y + 4, "3. Quit");
    ConsoleUtils::ResetTextColor();
    ConsoleUtils::PrintAt(x + 8, y + 4, "]");
    
    // 도움말
    ConsoleUtils::PrintCentered(height - 2, "Use Arrow Keys or 1/2/3 to select, Press Enter", ConsoleColor::CYAN);
}

void MainScreen::HandleInput(int key) {
    if (key == '1' || key == 49) {
        selectedOption_ = MainMenuOption::START_GAME;
    } else if (key == '2' || key == 50) {
        DisplayProfile();
        selectedOption_ = MainMenuOption::PROFILE;
    } else if (key == '3' || key == 51) {
        selectedOption_ = MainMenuOption::QUIT;
    } else if (key == 224) {  // 특수 키
        int nextKey = _getch();
        if (nextKey == 72) {  // UP
            currentSelection_ = (currentSelection_ - 1 + 3) % 3;
        } else if (nextKey == 80) {  // DOWN
            currentSelection_ = (currentSelection_ + 1) % 3;
        }
    } else if (key == '\r' || key == '\n') {
        switch (currentSelection_) {
            case 0:
                selectedOption_ = MainMenuOption::START_GAME;
                break;
            case 1:
                DisplayProfile();
                selectedOption_ = MainMenuOption::PROFILE;
                break;
            case 2:
                selectedOption_ = MainMenuOption::QUIT;
                break;
        }
    }
}

void MainScreen::DisplayProfile() {
    ConsoleUtils::Clear();
    
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    ConsoleUtils::DrawBorder();
    
    ConsoleUtils::PrintCentered(2, "=== Profile ===", ConsoleColor::YELLOW);
    
    int y = height / 3;
    int x = (width - 40) / 2;
    
    ConsoleUtils::PrintAt(x, y, "Nickname: " + gameState_->username);
    ConsoleUtils::PrintAt(x, y + 2, "Wins: 0");
    ConsoleUtils::PrintAt(x, y + 3, "Losses: 0");
    ConsoleUtils::PrintAt(x, y + 4, "Win Rate: 0.0%");
    
    ConsoleUtils::PrintCentered(height - 2, "Press any key to return...", ConsoleColor::CYAN);
    _getch();
}
