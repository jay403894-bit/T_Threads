#include "Clock.h"
using namespace T_Threads;
#ifdef _WIN32
Clock::Clock()
{
	QueryPerformanceFrequency(&frequency);
	reset();
}


void Clock::reset()
{
	QueryPerformanceCounter(&start);
}

float Clock::elapsedMs() const
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (now.QuadPart - start.QuadPart) * 1000.0 / static_cast<float>(frequency.QuadPart);
}

float Clock::elapsed() const
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (now.QuadPart - start.QuadPart) / static_cast<float>(frequency.QuadPart);
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

#endif