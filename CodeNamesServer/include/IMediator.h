#ifndef IMEDIATOR_H
#define IMEDIATOR_H

#include <memory>
#include <vector>
#include <string>
#include "Session.h"

class IMediator {
public:
    virtual ~IMediator() = default;
    
    // 세션 관리
    virtual bool AddSession(std::shared_ptr<Session> session) = 0;
    virtual void RemoveSession(SOCKET socket) = 0;
    
    // 게임 관리
    virtual void CreateGameRoom(const std::vector<std::shared_ptr<Session>>& players) = 0;
    virtual void RemoveGameRoom(const std::string& roomId) = 0;
};

#endif // IMEDIATOR_H