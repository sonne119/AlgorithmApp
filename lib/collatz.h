#ifndef COLLATZ_H
#define COLLATZ_H

#include <cstdint>
#include <string>

struct CollatzResult {
    uint64_t limit;
    double seconds;
    double throughput;
    uint64_t first_overflow;
    uint32_t longest_len;
    uint64_t longest_seed;
    uint64_t max_peak;
};

extern "C" int collatz_compute(uint64_t limit, CollatzResult& out);
int collatz_main(CollatzResult &res);
void build_cache();
std::string format_number(uint64_t num);


#ifdef __cplusplus
extern "C" {
#endif

int collatz_compute_and_write_pipe(int countThread,uint64_t limit, int result_fd, int log_fd);

#ifdef __cplusplus
}
#endif

#endif // COLLATZ_SOLVER_H
