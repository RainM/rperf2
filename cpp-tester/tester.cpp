#include <iostream>
#include <unordered_set>

#include "librperf2_cpp.hpp"

int main(int argc, char** argv) {
    std::unordered_set<int> some_set;

    int i = 0;
    while ((some_set.size() +1 ) >= argc) {
	_pt_profiler_start();

	some_set.insert(i++);

	_pt_profiler_stop();
    }

    return some_set.size();
}
