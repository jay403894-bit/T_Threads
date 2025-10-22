#pragma once
#include <iostream>
#include <chrono>
#include <source_location>
#include <format>
#include <fstream>
#include <string>
#include <mutex>  // For thread safety

enum class Log_Level : char {
    Info = 'I',
    Warning = 'W',
    Error = 'E'
};

class Log {
public:
    Log();
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log();

    void LogInfo(Log_Level level, const std::string& tStatus, std::source_location location = std::source_location::current());

private:

    std::ofstream log_file_;  // Declaration of the static member
    std::mutex log_mutex_;    // Mutex for thread safety
};