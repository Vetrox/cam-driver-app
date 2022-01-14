#pragma once


#include <chrono>
#define timeit_start	const auto __timeit__local__start = std::chrono::steady_clock::now();
#define timeit_diff_ms	std::chrono::duration_cast<std::chrono::milliseconds>(			\
		std::chrono::steady_clock::now() - __timeit__local__start						\
	).count()

#define timeit_diff_ns	std::chrono::duration_cast<std::chrono::nanoseconds>(			\
		std::chrono::steady_clock::now() - __timeit__local__start							\
	).count()