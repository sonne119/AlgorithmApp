#ifndef COLLATZ_SOLVER_H
#define COLLATZ_SOLVER_H
#include <string>
#include <cstdint>
#include <functional>
#include <limits>
#include <thread>
#include "../lib/collatz.h"

class CollatzRunner {
public:
    CollatzRunner();
    ~CollatzRunner();

    uint64_t limit = 9000000000;

    unsigned threadCount =  std::thread::hardware_concurrency();

    using LogCallback = std::function<void(const std::string&)>;

    CollatzResult Compute(LogCallback logCallback = nullptr);

private:
    CollatzResult r{};
};
#endif // COLLATZ_SOLVER_H
