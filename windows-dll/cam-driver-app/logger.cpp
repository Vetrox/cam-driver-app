#include <iostream>
#include <fstream>
#include "logger.h"

static std::ofstream logfile;
void cda_log(const char* msg)
{
	if (!logfile.is_open()) {
		logfile.open("C:\\Users\\felix\\OneDrive\\Desktop\\virtcam\\logfile.log", std::ios_base::app); // TODO: replace this
	}
	logfile << msg << std::flush;
}

void cda_stop_log()
{
	if (logfile.is_open()) {
		logfile.close();
	}
}

