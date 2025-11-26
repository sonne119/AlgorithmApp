#include <iostream>
#include "app/CollatzRunner.h"

int main()
{
    CollatzRunner runner;
    runner.limit = 1000000; // 1 million

    auto res = runner.Compute([](const std::string &msg) {
        std::cout << msg; // print logs as they arrive
    });

    std::cout << "\n--- Result ---\n";
    std::cout << "Limit: " << res.limit << "\n";
    std::cout << "Seconds: " << res.seconds << "\n";
    std::cout << "Throughput: " << res.throughput << "\n";
    std::cout << "Max length: " << res.longest_len << " (seed=" << res.longest_seed << ")\n";
    std::cout << "Max peak: " << res.max_peak << "\n";
    if (res.first_overflow != std::numeric_limits<uint64_t>::max())
        std::cout << "Overflow seed: " << res.first_overflow << "\n";
    else
        std::cout << "Overflow seed: NONE\n";

    return 0;
}
