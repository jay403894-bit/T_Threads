#pragma once
#include "Log.h"

class Logger {
public:
    // Delete the constructor so no one else can create instances of this manager
    Logger() = delete;
    Logger(const Logger& other) = delete;
    Logger& operator=(const Logger& other) = delete;
    // Delete the constructor so no one else can create instances of this manager
    ~Logger() = default;

    // Singleton accessor
    static std::shared_ptr<Log> Get();
private:
    static std::shared_ptr<Log> logger_;  // The singleton TaskScheduler instance
};

