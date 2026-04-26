#include "patterns/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
std::string nowString() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t tt = std::chrono::system_clock::to_time_t(now);

	std::tm tm{};
#if defined(_WIN32)
	localtime_s(&tm, &tt);
#else
	localtime_r(&tt, &tm);
#endif

	std::ostringstream os;
	os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	return os.str();
}
}

Logger& Logger::instance() {
	static Logger logger;
	return logger;
}

void Logger::info(const std::string& message) {
	std::lock_guard<std::mutex> lock(mutex_);
	std::cout << "[INFO] " << nowString() << " | " << message << '\n';
}

void Logger::error(const std::string& message) {
	std::lock_guard<std::mutex> lock(mutex_);
	std::cerr << "[ERROR] " << nowString() << " | " << message << '\n';
}

