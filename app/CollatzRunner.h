#ifndef COLLATZ_SOLVER_H
#define COLLATZ_SOLVER_H
#include <string>
#include <cstdint>
#include <functional>
#include <limits>
#include "../lib/collatz.h"
#include "../lib/collatz_simd.h"

class CollatzRunner {
public:
    CollatzRunner();
    ~CollatzRunner();

    uint64_t limit = 9000000000;
    int threadCount = 12;

    using LogCallback = std::function<void(const std::string&)>;
    CollatzResult Compute(LogCallback logCallback = nullptr);
    CollatzResult Compute_simd(LogCallback logCallback = nullptr);

private:
    CollatzResult r{};
};
#endif // COLLATZ_SOLVER_H
