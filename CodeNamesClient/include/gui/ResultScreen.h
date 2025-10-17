#pragma once

#include <string>
#include <memory>
#include "../core/GameState.h"

enum class ResultType {
    WIN,
    LOSE,
    DRAW
};

class ResultScreen {
public:
    ResultScreen(std::shared_ptr<GameState> gameState);
    
    // 게임 결과 화면 표시
    void Show();
    
    // 결과 타입 설정
    void SetResultType(ResultType type) { resultType_ = type; }
    
    // 승리 팀 설정
    void SetWinnerTeam(int team) { winnerTeam_ = team; }

private:
    std::shared_ptr<GameState> gameState_;
    ResultType resultType_;
    int winnerTeam_;
    
    // 결과 화면 그리기
    void DrawResult();
    
    // 결과 메시지 생성
    std::string GetResultMessage() const;
};
