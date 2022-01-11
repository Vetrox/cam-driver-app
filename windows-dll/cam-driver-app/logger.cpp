#include <filesystem>
#include <iostream>
#include <fstream>
#include <mutex>

#include <windows.h>

#include "logger.h"

std::string get_userprofile() {
	DWORD bufferSize = 32767 * 2 + 1;
	std::string buff;
	buff.resize(bufferSize);
	bufferSize = GetEnvironmentVariableA("userprofile", &buff[0], bufferSize);
	buff.resize(bufferSize);
	return buff + "\\cda.log";
}

static std::ofstream	logfile;
static std::mutex		mu;

void cda::log(const char* msg) {
	if constexpr (ENABLE_LOGGING) {
		std::lock_guard<std::mutex> guard(mu);
		if (!logfile.is_open()) {
			logfile.open(get_userprofile(), std::ios_base::trunc);
		}
		logfile << msg << std::flush;
	}
}

void cda::log(std::string msg) {
	log(msg.c_str());
}

void cda::logln(std::string msg) {
	log(msg + "\n");
}

void cda::logln(const char* msg) {
	log(msg);
	log("\n");
}
