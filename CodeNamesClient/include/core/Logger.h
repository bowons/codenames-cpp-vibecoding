#pragma once

#include <string>

namespace Logger {
    // Initialize logger with file path (append mode). Safe to call multiple times.
    void Init(const std::string& filepath = "client.log");

    void Info(const std::string& msg);
    void Warn(const std::string& msg);
    void Error(const std::string& msg);
}
