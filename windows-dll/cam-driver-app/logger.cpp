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

void cda::log(long long msg) {
	log(std::to_string(msg));
}
void cda::log(long msg) {
	log(std::to_string(msg));
}
void cda::log(unsigned long long msg) {
	log(std::to_string(msg));
}
void cda::log(unsigned long msg) {
	log(std::to_string(msg));
}
void cda::log(double msg) {
	log(std::to_string(msg));
}
void cda::log(long double msg) {
	log(std::to_string(msg));
}
void cda::log(float msg) {
	log(std::to_string(msg));
}
void cda::log(int msg) {
	log(std::to_string(msg));
}
void cda::log(unsigned int msg) {
	log(std::to_string(msg));
}

void cda::logln(const char* msg) {
	log(msg);
	log("\n");
}

void cda::logln(std::string msg) {
	logln(msg.c_str());
}

void cda::logln(long long msg) {
	logln(std::to_string(msg));
}
void cda::logln(long msg) {
	logln(std::to_string(msg));
}
void cda::logln(unsigned long long msg) {
	logln(std::to_string(msg));
}
void cda::logln(unsigned long msg) {
	logln(std::to_string(msg));
}
void cda::logln(double msg) {
	logln(std::to_string(msg));
}
void cda::logln(long double msg) {
	logln(std::to_string(msg));
}
void cda::logln(float msg) {
	logln(std::to_string(msg));
}
void cda::logln(int msg) {
	logln(std::to_string(msg));
}
void cda::logln(unsigned int msg) {
	logln(std::to_string(msg));
}