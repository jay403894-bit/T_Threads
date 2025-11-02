#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip> // For formatting output
#include <mutex>
#include <atomic>
#ifdef _WIN32
#include <windows.h>
namespace T_Threads {
    class TaskScheduler;
    class EpochManager;
    class Clock
    {
    public:
        Clock();
        double elapsedMs() const;
        double elapsed() const;
        std::string toString() const;

    private:
        friend class TaskScheduler;
        friend class EpochManager;
        void reset();
        LARGE_INTEGER start{};
        LARGE_INTEGER frequency{};
    };
#else

#endif
}