#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <memory>

class GameState;

class PacketHandler {
public:
    using PacketCallback = std::function<void(const std::string&)>;

    // 암시적 생성자를 방지한다
    explicit PacketHandler(std::shared_ptr<GameState> gameState);

    // 패킷 타입 별 핸들러 등록
    void RegisterHandler(const std::string& packetType, PacketCallback callback);

    // 수신한 패킷 처리
    void ProcessPacket(const std::string& packet);

    // 기본 핸들러 등록
    void RegisterDefaultHandlers();

private:
    std::shared_ptr<GameState> gameState_;
    std::unordered_map<std::string, PacketCallback> handlers_;

    // 기본 패킷 핸들러

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

    // 에러용
    void HandleError(const std::string& data);

    // 패킷 데이터 파싱
    std::string ParseField(const std::string& data, int fieldIndex, const std::string& delimiter = "|");
};