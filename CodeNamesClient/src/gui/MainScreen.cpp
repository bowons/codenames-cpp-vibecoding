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
        bool confirmed = HandleInput(key);
        if (confirmed) {
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
    // START GAME
    if (currentSelection_ == 0) {
        ConsoleUtils::PrintAt(x, y, ">  ");
    } else {
        ConsoleUtils::PrintAt(x, y, "   ");
    }
    ConsoleUtils::PrintAt(x + 3, y, "1. Start Game");
    
    // QUIT
    if (currentSelection_ == 1) {
        ConsoleUtils::PrintAt(x, y + 2, ">  ");
    } else {
        ConsoleUtils::PrintAt(x, y + 2, "   ");
    }
    ConsoleUtils::PrintAt(x + 3, y + 2, "2. Quit");
    
    // 도움말
    ConsoleUtils::PrintCentered(height - 2, "Use Arrow Keys or 1/2 to select, Press Enter", ConsoleColor::CYAN);
}

bool MainScreen::HandleInput(int key) {
    if (key == '1' || key == 49) {
        selectedOption_ = MainMenuOption::START_GAME;
        return true;
    } else if (key == '2' || key == 50) {
        selectedOption_ = MainMenuOption::QUIT;
        return true;
    } else if (key == 224) {  // 특수 키
        int nextKey = _getch();
        if (nextKey == 72) {  // UP
            currentSelection_ = (currentSelection_ - 1 + 2) % 2;
        } else if (nextKey == 80) {  // DOWN
            currentSelection_ = (currentSelection_ + 1) % 2;
        }
        return false;
    } else if (key == '\r' || key == '\n') {
        switch (currentSelection_) {
            case 0:
                selectedOption_ = MainMenuOption::START_GAME;
                return true;
            case 1:
                selectedOption_ = MainMenuOption::QUIT;
                return true;
        }
    }

    return false;
}

// Profile display removed (not implemented)
