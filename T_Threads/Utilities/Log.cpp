#include "Log.h"

//default constructor
Log::Log() {
    log_file_.open("log.txt", std::ios::out | std::ios::trunc);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log.txt for writing!" << std::endl;
    }
}
//destructor
Log::~Log() {
    if (log_file_.is_open()) {
        log_file_.flush();  // Ensure the data is written
        log_file_.close();  // Close the file when done
    }
}
//log info
void Log::LogInfo(Log_Level level, const std::string& message, std::source_location location) {
    if (!Log::log_file_.is_open()) {
        std::cerr << "Log file not open, skipping log entry." << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(log_mutex_);  // Ensure thread safety

    // Get current time in a readable format
    auto tp = std::chrono::system_clock::now();
    auto local_tp = std::chrono::zoned_time{ std::chrono::current_zone(), tp };
    std::string level_str;

    // Determine the LogInfo level string
    switch (level) {
    case Log_Level::Info:
        level_str = "INFO: ";
        break;
    case Log_Level::Warning:
        level_str = "WARNING: ";
        break;
    case Log_Level::Error:
        level_str = "ERROR: ";
        break;
    }

    try {
        // Write to the LogInfo file
        log_file_ << "[" << level_str << "] "
            << std::format("{:%F %T %Z}", local_tp)  // Format time
            << " | " << location.file_name()
            << ":" << location.function_name()
            << ":" << location.line()
            << " | " << message
            << std::endl;

        // Flush after writing to ensure it's written immediately
        log_file_.flush();
    }
    catch (const std::exception& e) {
        std::cerr << "Logger format error: " << e.what() << std::endl;
    }
}