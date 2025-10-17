#include "DatabaseManager.h"
#include "Session.h"

#include <iostream>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <functional>
#include <random>

const std::string DatabaseManager::DEV_MASTER_SALT = "dev_master_salt_12345"; // 개발용 하드코딩 솔트 (SSL 대체)

DatabaseManager::DatabaseManager() : db_(nullptr) 
{
}

DatabaseManager::~DatabaseManager() 
{
    Cleanup();
}

// 싱글톤 구현
DatabaseManager& DatabaseManager::GetInstance() {
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::InitializeSingleton(const std::string& dbPath) {
    return GetInstance().Initialize(dbPath);
}

bool DatabaseManager::Initialize(const std::string& dbPath)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    dbPath_ = dbPath;
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) 
    {
        std::cerr << "데이터베이스 열기 실패: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    return true;
}

void DatabaseManager::Cleanup() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

// Salt 생성 (C++ 표준 라이브러리 사용)
std::string DatabaseManager::GenerateSalt() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string salt;
    salt.reserve(SALT_LEN - 1);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    for (int i = 0; i < SALT_LEN - 1; ++i) {
        salt += alphanum[dis(gen)];
    }
    return salt;
}

// 비밀번호 + Salt 해시 (기존 C 코드와 동일한 방식 모사)
// 주의: SHA256 대신 std::hash 사용으로 기존 DB와 호환되지 않을 수 있음
std::string DatabaseManager::HashPasswordWithSalt(const std::string& password, const std::string& salt) {
    std::string salted_pw = password + salt;
    
    // 기존 C 코드처럼 단일 해시 (SHA256 대신 std::hash 사용)
    std::hash<std::string> hasher;
    size_t hash_value = hasher(salted_pw);
    
    // 64자리 16진수 문자열로 변환 (기존 SHA256 출력 형태 모사)
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // size_t를 64자리로 확장 (부족한 부분은 반복으로 채움)
    std::string base_hash = ss.str();
    ss.str("");
    ss << std::hex << hash_value;
    std::string hash_str = ss.str();
    
    // 64자리까지 확장
    while (hash_str.length() < 64) {
        hash_str += hash_str;
    }
    return hash_str.substr(0, 64);
}

bool DatabaseManager::CheckNicknameExists(const std::string& nickname) 
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    std::string query = "SELECT nickname FROM users WHERE nickname = ?";\
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "DB prepare error in CheckNicknameExists: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

std::optional<struct UserInfo> DatabaseManager::GetUserInfoByToken(const std::string &id)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return std::nullopt;

    // users와 game_stats 테이블 조인 후 모든 정보 조회
    std::string query = R"(
        SELECT u.id, u.nickname, u.report_count, u.is_suspended, 
               COALESCE(g.wins, 0) as wins, COALESCE(g.losses, 0) as losses
        FROM users u 
        LEFT JOIN game_stats g ON u.id = g.user_id 
        WHERE u.id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) 
    {
        std::cerr << "DB prepare error in GetUserInfo: " << sqlite3_errmsg(db_) << std::endl;
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt; // 사용자 없음
    }

    UserInfo userInfo;
    userInfo.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    userInfo.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    userInfo.report_count = sqlite3_column_int(stmt, 2);
    userInfo.is_suspended = sqlite3_column_int(stmt, 3) != 0;
    userInfo.wins = sqlite3_column_int(stmt, 4);
    userInfo.losses = sqlite3_column_int(stmt, 5);
    
    sqlite3_finalize(stmt);
    return userInfo;
}

std::string DatabaseManager::GenerateToken(int length)
{
    const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string token;
    token.reserve(length);

    std::random_device rd; // 난수 시드 생성
    std::mt19937 gen(rd()); // 난수 생성기 초기화
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2); // -2는 널문자 제외

    for (int i = 0; i < length; ++i) {
        token += alphanum[dis(gen)];
    }
    return token;
}

DatabaseResult DatabaseManager::LoginUser(const std::string& id, const std::string& pw)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return DatabaseResult::DB_ERROR;

    std::string query = "SELECT pw, salt, is_suspended FROM users WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "DB prepare error in LoginUser: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return DatabaseResult::NOT_FOUND; // 없는 계정
    }

    std::string stored_pw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    std::string salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    int is_suspended = sqlite3_column_int(stmt, 2);

    sqlite3_finalize(stmt);

    if (is_suspended) {
        return DatabaseResult::SUSPENDED; // 정지된 계정
    }

    // 비밀번호 검증
    std::string hashed_input_pw = HashPasswordWithSalt(pw, salt);
    if (hashed_input_pw != stored_pw) {
        return DatabaseResult::WRONG_PASSWORD; // 비밀번호 불일치
    }

    return DatabaseResult::SUCCESS;
}

DatabaseResult DatabaseManager::SignupUser(const std::string& id, const std::string& pw, const std::string& nickname) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return DatabaseResult::DB_ERROR;
    
    // 트랜잭션 시작
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "트랜잭션 시작 실패: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }
    
    // 중복 검사 (아이디 또는 닉네임)
    std::string check_query = "SELECT id FROM users WHERE id = ? OR nickname = ?";
    sqlite3_stmt* check_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, check_query.c_str(), -1, &check_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return DatabaseResult::DB_ERROR;
    }
    
    sqlite3_bind_text(check_stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(check_stmt, 2, nickname.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(check_stmt) == SQLITE_ROW) {
        sqlite3_finalize(check_stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return DatabaseResult::NICK_DUPLICATE; // 중복된 아이디 또는 닉네임
    }
    sqlite3_finalize(check_stmt);
    
    // Salt 생성 및 패스워드 해싱
    std::string salt = GenerateSalt();
    std::string hashed_pw = HashPasswordWithSalt(pw, salt);
    
    // users 테이블 삽입
    std::string insert_user = "INSERT INTO users(id, pw, salt, nickname, report_count, is_suspended) VALUES (?, ?, ?, ?, 0, 0)";
    sqlite3_stmt* user_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insert_user.c_str(), -1, &user_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return DatabaseResult::DB_ERROR;
    }
    
    sqlite3_bind_text(user_stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(user_stmt, 2, hashed_pw.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(user_stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(user_stmt, 4, nickname.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(user_stmt) != SQLITE_DONE) {
        sqlite3_finalize(user_stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return DatabaseResult::DB_ERROR;
    }
    sqlite3_finalize(user_stmt);
    
    // game_stats 테이블 삽입
    std::string insert_stats = "INSERT OR IGNORE INTO game_stats (user_id, wins, losses) VALUES (?, 0, 0)";
    sqlite3_stmt* stats_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insert_stats.c_str(), -1, &stats_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return DatabaseResult::DB_ERROR;
    }
    
    sqlite3_bind_text(stats_stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stats_stmt) != SQLITE_DONE) {
        sqlite3_finalize(stats_stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return DatabaseResult::DB_ERROR;
    }
    sqlite3_finalize(stats_stmt);
    
    // 트랜잭션 커밋
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "트랜잭션 커밋 실패: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return DatabaseResult::DB_ERROR;
    }
    
    std::cout << "사용자 생성 성공: " << id << std::endl;
    return DatabaseResult::SUCCESS;
}

DatabaseResult DatabaseManager::ChangePassword(const std::string& id, const std::string& newPassword) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    
    std::string salt = GenerateSalt();
    std::string hashedPassword = HashPasswordWithSalt(newPassword, salt);

    const char* sql = "UPDATE users SET password = ?, salt = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB prepare error in ChangePassword: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }

    sqlite3_bind_text(stmt, 1, hashedPassword.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, salt.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, id.c_str(), -1, SQLITE_STATIC);
    
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

        if (result == SQLITE_DONE && sqlite3_changes(db_) > 0) {
        return DatabaseResult::SUCCESS;
    }
    return DatabaseResult::NOT_FOUND;
}

DatabaseResult DatabaseManager::ChangeNickname(const std::string& id, const std::string& newNickname) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    // 닉네임 중복 체크
    const char* checkSql = "SELECT COUNT(*) FROM users WHERE nickname = ? AND id != ?";
    sqlite3_stmt* checkStmt;

    if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB prepare error in ChangeNickname (check): " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }

    
    sqlite3_bind_text(checkStmt, 1, newNickname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(checkStmt, 2, id.c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        count = sqlite3_column_int(checkStmt, 0);
    }
    sqlite3_finalize(checkStmt);
    
    if (count > 0) {
        return DatabaseResult::NICK_DUPLICATE; // 닉네임 중복
    }

    const char* updateSql = "UPDATE users SET nickname = ? WHERE id = ?";
    sqlite3_stmt* updateStmt;

    if (sqlite3_prepare_v2(db_, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB prepare error in ChangeNickname: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }
        
    sqlite3_bind_text(updateStmt, 1, newNickname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(updateStmt, 2, id.c_str(), -1, SQLITE_STATIC);
    
    int result = sqlite3_step(updateStmt);
    sqlite3_finalize(updateStmt);
    
    if (result == SQLITE_DONE && sqlite3_changes(db_) > 0) {
        return DatabaseResult::SUCCESS;
    }
    return DatabaseResult::NOT_FOUND;
}

DatabaseResult DatabaseManager::WithdrawUser(const std::string& id) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    const char* sql = "DELETE FROM users WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB prepare error in WithdrawUser: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result == SQLITE_DONE && sqlite3_changes(db_) > 0) {
        return DatabaseResult::SUCCESS;
    }
    return DatabaseResult::NOT_FOUND;
}

DatabaseResult DatabaseManager::ReportUser(const std::string& id) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    const char* sql = "UPDATE users SET report_count = report_count + 1 WHERE id = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB prepare error in ReportUser: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result == SQLITE_DONE && sqlite3_changes(db_) > 0) {
        int reportCount = GetReportCount(id);
        if (reportCount >= 5) {
            SuspendUser(id);
        }
        return DatabaseResult::SUCCESS;
    }
    return DatabaseResult::NOT_FOUND;
}

DatabaseResult DatabaseManager::SuspendUser(const std::string& id) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    const char* sql = "UPDATE users SET is_suspended = 1 WHERE id = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB prepare error in SuspendUser: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (result == SQLITE_DONE && sqlite3_changes(db_) > 0) {
        return DatabaseResult::SUCCESS;
    }
    return DatabaseResult::NOT_FOUND;
}

bool DatabaseManager::IsAccountSuspended(const std::string& id) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    const char* sql = "SELECT is_suspended FROM users WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    
    bool suspended = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        suspended = sqlite3_column_int(stmt, 0) == 1;
    }
    
    sqlite3_finalize(stmt);
    return suspended;
}

int DatabaseManager::GetReportCount(const std::string& id) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    const char* sql = "SELECT report_count FROM users WHERE id = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count =  sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

DatabaseResult DatabaseManager::SaveGameResult(const std::string& nickname, const std::string& result) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    const char* sql;
    if (result == "WIN") {
        sql = "UPDATE users SET wins = wins + 1 WHERE nickname = ?";
    } else if (result == "LOSS") {
        sql = "UPDATE users SET losses = losses + 1 WHERE nickname = ?";
    } else {
        return DatabaseResult::DB_ERROR;
    }

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DB prepare error in SaveGameResult: " << sqlite3_errmsg(db_) << std::endl;
        return DatabaseResult::DB_ERROR;
    }

    sqlite3_bind_text(stmt, 1, nickname.c_str(), -1, SQLITE_STATIC);

    int stepResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (stepResult == SQLITE_DONE && sqlite3_changes(db_) > 0) {
        return DatabaseResult::SUCCESS;
    }
    return DatabaseResult::NOT_FOUND;
}

std::string DatabaseManager::GetNicknameByToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    
    const char* sql = "SELECT nickname FROM users WHERE id = ?"; // 토큰을 id로 사용
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }
    
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
    
    std::string nickname;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* nick = (const char*)sqlite3_column_text(stmt, 0);
        if (nick) {
            nickname = nick;
        }
    }
    
    sqlite3_finalize(stmt);
    return nickname;
}

bool DatabaseManager::CheckIdExists(const std::string& id) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    
    const char* sql = "SELECT COUNT(*) FROM users WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        exists = (count > 0);
    }
    
    sqlite3_finalize(stmt);
    return exists;
}