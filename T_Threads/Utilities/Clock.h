#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip> // For formatting output
#include <mutex>

#ifdef _WIN32
#include <windows.h>

class Clock
{
public:
    Clock();
    void reset();
    double elapsedMs() const;
    double elapsed() const;
    std::string toString() const;

private:
    LARGE_INTEGER start{};
    LARGE_INTEGER frequency{};
    std::mutex reset_mutex;
};
#else

#endif