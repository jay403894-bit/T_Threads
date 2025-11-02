#pragma once
#pragma once
#include <stdexcept>
#include <string>
namespace T_Threads {
    class TaskException : public std::runtime_error {
    public:
        explicit TaskException(const std::string& msg)
            : std::runtime_error("[TaskException] " + msg) {}
    };

    class ThreadException : public TaskException {
    public:
        explicit ThreadException(const std::string& msg)
            : TaskException("[Thread] " + msg) {
        }
    };

    class SchedulerException : public TaskException {
    public:
        explicit SchedulerException(const std::string& msg)
            : TaskException("[Scheduler] " + msg) {
        }
    };
}