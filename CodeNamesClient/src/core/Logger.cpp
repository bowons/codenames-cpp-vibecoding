#include "../../include/core/Logger.h"
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
// PID retrieval
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
    static std::ofstream logFile;
    static std::recursive_mutex logMutex; // 데드락 방지를 위해 재진입 가능 뮤텍스 사용
    static bool initialized = false;
    static std::string pidStr;

    std::string Timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
}

namespace Logger {
    void Init(const std::string& filepath) {
        std::lock_guard<std::recursive_mutex> lk(logMutex);
        if (initialized) return;
        // Determine PID and inject into filename so multiple processes
        // do not write to the same file.
#ifdef _WIN32
        unsigned long pid = GetCurrentProcessId();
#else
        unsigned long pid = static_cast<unsigned long>(getpid());
#endif
        pidStr = std::to_string(pid);

        // Insert -<pid> before extension, or append
        std::string fname = filepath;
        auto pos = fname.find_last_of('.');
        if (pos != std::string::npos) {
            fname = fname.substr(0,pos) + "-" + pidStr + fname.substr(pos);
        } else {
            fname += "-" + pidStr;
        }

        logFile.open(fname, std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            initialized = true;
            logFile << "[" << Timestamp() << "] " << "Logger initialized" << std::endl;
            logFile.flush();
        }
    }

    static void Write(const std::string& level, const std::string& msg) {
        std::lock_guard<std::recursive_mutex> lk(logMutex);
        if (!initialized) {
            // fallback: try to initialize default file
            Init("client.log");
        }
        if (logFile.is_open()) {
            logFile << "[" << Timestamp() << "] [" << level << "] " << msg << std::endl;
            logFile.flush();
        }
    }

    void Info(const std::string& msg) { Write("INFO", msg); }
    void Warn(const std::string& msg) { Write("WARN", msg); }
    void Error(const std::string& msg) { Write("ERROR", msg); }
}
