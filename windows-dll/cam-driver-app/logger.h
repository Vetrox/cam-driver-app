#pragma once

#include <string>

namespace cda {
	constexpr auto ENABLE_LOGGING = true;
	void log(const char* msg);
	void log(std::string msg);
	void log(long long msg);
	void log(long msg);
	void log(unsigned long long msg);
	void log(unsigned long msg);
	void log(double msg);
	void log(long double msg);
	void log(float msg);
	void log(int msg);
	void log(unsigned int msg);
	
	void logln(const char* msg);
	void logln(std::string msg);
	void logln(long long msg);
	void logln(long msg);
	void logln(unsigned long long msg);
	void logln(unsigned long msg);
	void logln(double msg);
	void logln(long double msg);
	void logln(float msg);
	void logln(int msg);
	void logln(unsigned int msg);
}