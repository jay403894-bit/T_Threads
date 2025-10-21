#pragma once
#include <chrono>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip> // For formatting output
#include <mutex>

class Clock {
public:
    // Default constructor - Starts with the clock paused
    Clock();
    // Destructor
    ~Clock();
    // Copy constructor - Keeps the clock paused
    Clock(const Clock& other);
    // Move constructor - Keeps the clock paused after moving
    Clock(Clock&& other) noexcept;
    // Move assignment operator
    Clock& operator=(Clock&& other) noexcept;
    // Copy assignment operator
    Clock& operator=(const Clock& other);
    // Convert the current time duration to a string formatted as "HH:MM:SS.MS"
    std::string ToString() const;
    // Stop the clock (accumulate the time)

    long long ElapsedMS() const;

    double Elapsed() const;
    // Start the clock (begin measuring time)
    void Start();
    std::chrono::steady_clock::time_point Now() const;
private:
    void Stop();
    // Resume the clock without restart 
    void Resume();
        // Reset the clock (clears the accumulated time)
    void Reset();
    // Get the elapsed time in milliseconds
    std::chrono::time_point<std::chrono::steady_clock> m_start; // Start timepoint
    std::chrono::time_point<std::chrono::steady_clock> m_end;  // end timepoint
    std::chrono::steady_clock::duration m_duration;           // Using steady_clock::duration for simplicity
    bool m_paused;                                           // clock timer pause flag
    mutable std::mutex timer_mutex;                         // Mutex for thread-safety
};