#pragma once

#include <mutex>
#include <string>

class Logger {
public:
	static Logger& instance();

	void info(const std::string& message);
	void error(const std::string& message);

private:
	Logger() = default;
	std::mutex mutex_;
};

