#pragma once

#ifdef DEBUG
	#define CDA_LOG(x) cda_log(x)
	#define CDA_CLOSE_LOG() cda_stop_log()
#else
	#define CDA_LOG(X)
	#define CDA_CLOSE_LOG()
#endif

void cda_log(const char* msg);
void cda_stop_log();
