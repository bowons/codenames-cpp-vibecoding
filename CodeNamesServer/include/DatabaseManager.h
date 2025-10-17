#pragma once

#include <string>
#include <memory>
#include <sqlite3.h>
#include <mutex>
#include <optional>

enum class DatabaseResult {
    SUCCESS = 0,           // 성공
    DB_ERROR = -1,         // 일반 DB 오류  
    NICK_DUPLICATE = -2,        // 회원가입: 아이디/닉네임 중복
    NOT_FOUND = -5,        // 로그인: 존재하지 않는 계정
    WRONG_PASSWORD = -3,   // 비밀번호 불일치
    SUSPENDED = -4         // 정지된 계정
};

class DatabaseManager {
private:
    sqlite3* db_;
    std::string dbPath_;
    std::mutex dbMutex_;

    static const std::string DEV_MASTER_SALT; // 개발용 마스터 솔트
    
    // 싱글톤으로 구현
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&&) = delete;
    DatabaseManager& operator=(DatabaseManager&&) = delete;

public:
    static DatabaseManager& GetInstance();
    static bool InitializeSingleton(const std::string& dbPath);

    bool Initialize(const std::string& dbPath);
    void Cleanup();

    // 인증 관련
    DatabaseResult LoginUser(const std::string& id, const std::string& pw);
    DatabaseResult SignupUser(const std::string& id, const std::string& pw, const std::string& nickname);
    
    // 중복 체크
    bool CheckIdExists(const std::string& id);
    bool CheckNicknameExists(const std::string& nickname);
    std::optional<struct UserInfo> GetUserInfoByToken(const std::string& id);

    std::string GenerateToken(int length = 32);

    // 사용자 정보 수정
    DatabaseResult ChangePassword(const std::string& id, const std::string& newPassword);
    DatabaseResult ChangeNickname(const std::string& id, const std::string& newNickname);
    DatabaseResult WithdrawUser(const std::string& id);
    
    // 신고 시스템
    DatabaseResult ReportUser(const std::string& id);
    DatabaseResult SuspendUser(const std::string& id);
    bool IsAccountSuspended(const std::string& id);
    int GetReportCount(const std::string& id);

    // 게임 결과
    DatabaseResult SaveGameResult(const std::string& nickname, const std::string& result); // "WIN" or "LOSS"

    // 추가 조회 점수
    std::string GetNicknameByToken(const std::string& token);

private:
    // 암호화 관련 상수
    static const int SALT_LEN = 16;
    static const int HASH_LEN = 65;
    
    // 헬퍼 함수들
    std::string GenerateSalt();
    std::string HashPasswordWithSalt(const std::string& password, const std::string& salt);
};