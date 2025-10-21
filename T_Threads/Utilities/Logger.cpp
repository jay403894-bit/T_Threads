#include "Logger.h"

//pointer to a log
std::shared_ptr<Log> Logger::logger_ = nullptr;
//get an instance of the logger
std::shared_ptr<Log> Logger::Get() {
	if (!logger_)
		logger_ = std::make_shared<Log>();
	return logger_;
}
