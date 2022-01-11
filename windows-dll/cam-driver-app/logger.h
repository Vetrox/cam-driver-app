#pragma once

#include <string>

namespace cda {
	constexpr auto ENABLE_LOGGING = false;
	void log(const char* msg);
	void log(std::string msg);
	void logln(const char* msg);
	void logln(std::string msg);
}