#include "../../include/gui/ResultScreen.h"
#include "../../include/gui/ConsoleUtils.h"
#include <iostream>
#include <conio.h>

ResultScreen::ResultScreen(std::shared_ptr<GameState> gameState)
    : gameState_(gameState),
      resultType_(ResultType::DRAW),
      winnerTeam_(-1) {
}

void ResultScreen::Show() {
    ConsoleUtils::Initialize();
    ConsoleUtils::Clear();
    
    DrawResult();
    
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    ConsoleUtils::PrintCentered(
        height - 1, 
        "Press any key to continue...", 
        ConsoleColor::CYAN
    );
    
    _getch();
}

void ResultScreen::DrawResult() {
    int width, height;
    ConsoleUtils::GetConsoleSize(width, height);
    
    ConsoleUtils::DrawBorder();
    
    ConsoleUtils::PrintCentered(2, "=== GAME RESULT ===", ConsoleColor::YELLOW);
    
    int y = height / 3;
    
    std::string result = GetResultMessage();
    ConsoleColor resultColor = (resultType_ == ResultType::WIN) ? ConsoleColor::GREEN : ConsoleColor::RED;
    
    ConsoleUtils::PrintCentered(y, result, resultColor);
    
    // 최종 점수
    ConsoleUtils::PrintCentered(y + 3, 
        "RED: " + std::to_string(gameState_->redScore) + 
        " | BLUE: " + std::to_string(gameState_->blueScore),
        ConsoleColor::WHITE);
    
    // 우승팀
    if (winnerTeam_ >= 0) {
        std::string winner = (winnerTeam_ == 0) ? "RED TEAM" : "BLUE TEAM";
        ConsoleUtils::PrintCentered(y + 5, "Winner: " + winner, ConsoleColor::YELLOW);
    }
}

std::string ResultScreen::GetResultMessage() const {
    switch (resultType_) {
        case ResultType::WIN:
            return "YOU WIN!";
        case ResultType::LOSE:
            return "YOU LOSE!";
        case ResultType::DRAW:
            return "GAME OVER";
        default:
            return "END GAME";
    }
}
