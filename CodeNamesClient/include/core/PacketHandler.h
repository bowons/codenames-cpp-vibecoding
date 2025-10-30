#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <memory>

class GameState;

class PacketHandler {
public:
    using PacketCallback = std::function<void(const std::string&)>;

    explicit PacketHandler(std::shared_ptr<GameState> gameState);

    // 패킷 타입별 핸들러 등록
    void RegisterHandler(const std::string& packetType, PacketCallback callback);

    // 수신한 패킷 처리
    void ProcessPacket(const std::string& packet);

    // 기본 핸들러들 등록 (자동 호출)
    void RegisterDefaultHandlers();

private:
    std::shared_ptr<GameState> gameState_;
    std::unordered_map<std::string, PacketCallback> handlers_;

    // ==================== 기본 패킷 핸들러들 ====================
    // 인증
    void HandleSignupOk(const std::string& data);
    void HandleLoginOk(const std::string& data);
    void HandleInvalidToken(const std::string& data);

    // 사용자 정보
    void HandleUserProfile(const std::string& data);

    // 게임 프로토콜
    void HandleWaitReply(const std::string& data);
    void HandleQueueFull(const std::string& data);
    void HandleGameInit(const std::string& data);
    void HandleGameStart(const std::string& data);
    void HandleAllCards(const std::string& data);
    void HandleRoleInfo(const std::string& data);
    void HandleTurnUpdate(const std::string& data);
    void HandleHintMsg(const std::string& data);
    void HandleCardUpdate(const std::string& data);
    void HandleChatMsg(const std::string& data);

    // 기타
    void HandleError(const std::string& data);
    // 로그인 실패용 사용자 피드백 (계정 없음 / 비밀번호 틀림 등)
    void HandleLoginFailure(const std::string& reason);

    // ==================== 유틸리티 함수 ====================
    // 패킷 데이터 파싱
    std::string ParseField(const std::string& data, int fieldIndex, const std::string& delimiter = "|");
};
