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

    void Reset();
    // Returns elapsed milliseconds since last Reset()
    double ElapsedMS() const;
    // Returns elapsed seconds (as double)
    double Elapsed() const;

    // Human-readable time string (HH:MM:SS.MS)
    std::string ToString() const;

private:
    LARGE_INTEGER start{};
    LARGE_INTEGER frequency{};
    std::mutex reset_mutex;
};
#else

#endif