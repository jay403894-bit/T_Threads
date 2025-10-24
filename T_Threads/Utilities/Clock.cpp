#include "Clock.h"

#ifdef _WIN32
Clock::Clock()
{
	QueryPerformanceFrequency(&frequency);
	reset();
}

void Clock::reset()
{
	std::lock_guard<std::mutex> lock(reset_mutex);
	QueryPerformanceCounter(&start);
}

// Returns elapsed milliseconds since last reset()
double Clock::elapsedMs() const
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (now.QuadPart - start.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
}

// Returns elapsed seconds (as double)
double Clock::elapsed() const
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (now.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
}

// Human-readable time string (HH:MM:SS.MS)
std::string Clock::toString() const
{
	double ms_total = elapsedMs();

	auto hours = static_cast<int>(ms_total / (1000.0 * 60.0 * 60.0));
	auto minutes = static_cast<int>((ms_total / (1000.0 * 60.0))) % 60;
	auto seconds = static_cast<int>((ms_total / 1000.0)) % 60;
	auto milliseconds = static_cast<int>(fmod(ms_total, 1000.0));

	std::ostringstream ss;
	ss << std::setw(2) << std::setfill('0') << hours << ":"
		<< std::setw(2) << std::setfill('0') << minutes << ":"
		<< std::setw(2) << std::setfill('0') << seconds << "."
		<< std::setw(3) << std::setfill('0') << milliseconds;

	return ss.str();
}

#else

// Default constructor - Starts with the clock_ paused_
Clock::Clock()
	: m_start(std::chrono::steady_clock::now()),
	m_end(m_start),
	m_duration(0),  // Start with no accumulated duration
	m_paused(true) { // Start in paused_ state
}

// Copy constructor - Keeps the clock_ paused_
Clock::Clock(const Clock& other)
	: m_start(other.m_start),
	m_end(other.m_end),
	m_duration(other.m_duration),
	m_paused(true) { // Copy constructor keeps the clock_ paused_
}

// Move constructor - Keeps the clock_ paused_ after moving
Clock::Clock(Clock&& other) noexcept
	: m_start(std::move(other.m_start)),
	m_end(std::move(other.m_end)),
	m_duration(other.m_duration),
	m_paused(true) {
}

Clock& Clock::operator=(Clock&& other) noexcept {
	if (this != &other) {
		std::lock_guard<std::mutex> lock(timer_mutex);
		m_start = std::move(other.m_start);
		m_end = std::move(other.m_end);
		m_duration = other.m_duration;
		m_paused = other.m_paused;
		other.m_duration = m_duration;
		other.m_paused = false;
		other.m_start = std::chrono::steady_clock::time_point();
		other.m_end = std::chrono::steady_clock::time_point();
	}
	return *this;
}
// Copy assignment operator
Clock& Clock::operator=(const Clock& other) {
	std::lock_guard<std::mutex> lock(timer_mutex);
	if (this != &other) {
		std::lock_guard<std::mutex> lock(timer_mutex);
		m_start = other.m_start;
		m_end = other.m_end;
		m_duration = other.m_duration;
		m_paused = other.m_paused;
	}
	return *this;
}
// Destructor
Clock::~Clock() {}
// Convert the current time duration to a string formatted as "HH:MM:SS.MS"
std::string Clock::toString() const {
	auto ms_total = ElapsedMS();

	auto hours = ms_total / (1000 * 60 * 60);
	auto minutes = (ms_total / (1000 * 60)) % 60;
	auto seconds = (ms_total / 1000) % 60;
	auto milliseconds = ms_total % 1000;

	std::ostringstream ss;
	ss << std::setw(2) << std::setfill('0') << hours << ":"
		<< std::setw(2) << std::setfill('0') << minutes << ":"
		<< std::setw(2) << std::setfill('0') << seconds << "."
		<< std::setw(3) << std::setfill('0') << milliseconds;

	return ss.str();
}
// Stop the clock_ (accumulate the time)
void Clock::Stop() {
	std::lock_guard<std::mutex> lock(timer_mutex);
	if (!m_paused) {
		m_end = std::chrono::steady_clock::now();
		auto time_elapsed = m_end - m_start;
		m_duration += time_elapsed;
		m_paused = true;
	}
}
// Resume the clock_ (continue from the last Start time without resetting Start)
void Clock::Resume() {
	std::lock_guard<std::mutex> lock(timer_mutex);
	if (m_paused) {
		m_start = std::chrono::steady_clock::now() - (m_end - m_start);
		m_paused = false;
	}
}
// Start the clock_ (begin measuring time)
void Clock::Start() {
	std::lock_guard<std::mutex> lock(timer_mutex);
	if (m_paused) {
		m_start = std::chrono::steady_clock::now();
		m_end = m_start;
		m_paused = false;
	}
}
// reset the clock_ (clears the accumulated time)
void Clock::reset() {
	std::lock_guard<std::mutex> lock(timer_mutex);
	m_end = std::chrono::steady_clock::now();
	m_start = m_end;
	m_duration = std::chrono::steady_clock::duration::zero();
	m_paused = true;
}
// instance the elapsed time in milliseconds
long long Clock::ElapsedMS() const {
	std::lock_guard<std::mutex> lock(timer_mutex);

	if (m_paused) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(m_duration).count();
	}
	else {
		auto Now = std::chrono::steady_clock::now();
		auto total_elapsed = m_duration + (Now - m_start);
		return std::chrono::duration_cast<std::chrono::milliseconds>(total_elapsed).count();
	}
}
double Clock::elapsed() const {
	std::lock_guard<std::mutex> lock(timer_mutex);

	if (m_paused) {
		return std::chrono::duration_cast<std::chrono::duration<double>>(m_duration).count();
	}
	else {
		auto Now = std::chrono::steady_clock::now();
		auto total_elapsed = m_duration + (Now - m_start);
		return std::chrono::duration_cast<std::chrono::duration<double>>(total_elapsed).count();
	}
}

std::chrono::steady_clock::time_point Clock::Now() const {
	std::lock_guard<std::mutex> lock(timer_mutex);
	return std::chrono::steady_clock::now();
}
#endif