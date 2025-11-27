#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <map>
#include <limits>
#include <bit>
#include <unistd.h>
#include <sys/types.h>
#include "collatz.h"
#include "platform_compat.h"
// #include <boost/asio/thread_pool.hpp>
// #include <boost/asio/post.hpp>

static std::atomic<bool> collatz_logging_enabled{true};
static std::atomic<int> global_log_fd{-1};


// ================= HELPER ========================
// write to log pipe
static void write_to_log(const std::string& message) {
    int log_fd = global_log_fd.load(std::memory_order_relaxed);
    if (log_fd != -1) {
        write(log_fd, message.c_str(), message.length());
    }
    std::cout << message << std::flush;
}


// ================= CONFIGURATION =================
constexpr uint64_t CACHE_LIMIT = 1ULL << 27; // 128 MB
constexpr size_t HIST_SIZE =4096;

// Global Cache
std::vector<uint16_t> collatz_cache;

// Global Results
std::atomic<uint64_t> global_first_overflow(INT64_MAX);
std::atomic<uint64_t> global_max_peak(0);
std::atomic<uint64_t> global_longest_seed(1);
std::atomic<uint32_t> global_longest_len(0);

// Global Histogram
std::mutex histogram_mutex;
std::map<uint32_t, uint64_t> global_histogram_map;

// ================= HELPERS =================


#if defined(__has_include)
#  if __has_include(<bit>)
#    include <bit>
#    define HAS_STD_COUNTR_ZERO 1
#  endif
#endif

template<typename T>
inline void atomic_update_max(std::atomic<T>& atom, T val) {
    T cur = atom.load(std::memory_order_relaxed);
    while (val > cur) if (atom.compare_exchange_weak(cur, val, std::memory_order_relaxed)) break;
}

template<typename T>
inline void atomic_update_min(std::atomic<T>& atom, T val) {
    T cur = atom.load(std::memory_order_relaxed);
    while (val < cur) if (atom.compare_exchange_weak(cur, val, std::memory_order_relaxed)) break;
}
// alternative fast_ctz
inline int fast_ctz(uint64_t n) {
    if (n == 0) return 0;
#ifdef HAS_STD_COUNTR_ZERO
    return static_cast<int>(std::countr_zero(n));
#elif defined(_MSC_VER)
    unsigned long index;
#ifdef _WIN64
    _BitScanForward64(&index, n);
#else
    if ((n & 0xFFFFFFFF) != 0) {
        _BitScanForward(&index, static_cast<unsigned long>(n));
    } else {
        _BitScanForward(&index, static_cast<unsigned long>(n >> 32));
        index += 32;
    }
#endif
    return static_cast<int>(index);
#else
    return __builtin_ctzll(n);
#endif
}

// Threshold where 3*n+1 might overflow INT64_MAX
constexpr uint64_t SAFE_THRESHOLD = (static_cast<uint64_t>(INT64_MAX) - 1) / 3;

// ================= BUILD CACHE =================
void build_cache_parallel() {
    write_to_log("  > Building Cache ... ");
    auto start = std::chrono::high_resolution_clock::now();

    collatz_cache.resize(CACHE_LIMIT);
    collatz_cache[1] = 0;

    unsigned int threads = std::thread::hardware_concurrency();
    uint64_t phase_size = 100000;

    for (uint64_t phase_start = 2; phase_start < CACHE_LIMIT; phase_start += phase_size) {
        uint64_t phase_end = std::min(phase_start + phase_size, CACHE_LIMIT);
        std::vector<std::thread> workers;
        uint64_t chunk = (phase_end - phase_start + threads - 1) / threads;

        for(unsigned int t = 0; t < threads; ++t) {
            uint64_t s = phase_start + t * chunk;
            if (s >= phase_end) break;
            uint64_t e = std::min(s + chunk, phase_end);

            workers.emplace_back([s, e, phase_start]() {
                uint16_t* cache = collatz_cache.data();
                for (uint64_t i = s; i < e; ++i) {
                    uint64_t n = i;
                    uint16_t steps = 0;
                    while (n >= phase_start) {
                        if ((n & 1) == 0) {
                            int zeros = fast_ctz(n);
                            n >>= zeros;
                            steps += zeros;
                        } else {
                            n = (n * 3 + 1) >> 1;
                            steps += 2;
                        }
                    }
                    cache[i] = steps + cache[n];
                }
            });
        }
        for(auto& t : workers) t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::ostringstream oss;
    oss << "Done (" << std::chrono::duration<double>(end - start).count() << "s)\n";
    write_to_log(oss.str());
}

// ================= WORKER LOGIC =================

struct alignas(128) ThreadResult {
    uint64_t histogram[HIST_SIZE] = {0};  // Local histogram
    uint64_t max_seed = 0;                // Seed with max length found
    uint64_t max_peak = 0;                // Max peak found
    uint64_t first_overflow = INT64_MAX;  // First seed causing overflow found b
    uint32_t max_length = 0;              // Max length found
};


static inline void step_hybrid(uint64_t& n, uint32_t& steps, uint64_t& peak,
                               uint64_t seed, uint64_t& overflow) {
    if (n >= CACHE_LIMIT) {
        if (n < SAFE_THRESHOLD) {
            uint64_t next_val = n * 3 + 1;
            if (next_val > peak) peak = next_val;
            int zeros = fast_ctz(next_val);
            n = next_val >> zeros;
            steps += 1 + zeros;
        } else {
#ifdef NO_INT128
            if (n < SAFE_THRESHOLD) {
                if (seed < overflow) overflow = seed;
                n = 0; // Stop signal
                return;
            }
            uint64_t next_val = n * 3 + 1;
            if (next_val > peak) peak = next_val;
            int zeros = fast_ctz(next_val);
            n = next_val >> zeros;
            steps += 1 + zeros;
#else
            unsigned __int128 wide = (unsigned __int128)n * 3 + 1;
            if (wide > (unsigned __int128)INT64_MAX) {
                if (seed < overflow) overflow = seed;
                n = 0; // Stop signal
                return;
            }
            uint64_t next_val = (uint64_t)wide;
            if (next_val > peak) peak = next_val;
            int zeros = fast_ctz(next_val);
            n = next_val >> zeros;
            steps += 1 + zeros;
#endif
        }
    }
}


void worker_static(uint64_t start, uint64_t end, int thread_id) {
    ThreadResult res;
    const uint16_t* __restrict__ cache = collatz_cache.data();

    if ((start & 1) == 0) start++;
    if (start <= 1) start = 3;

    uint64_t i = start;

    // --- 8-WAY HYBRID MATH ---
    for (; i + 14 <= end; i += 16) {
        uint64_t n0 = i,      n1 = i+2,    n2 = i+4,    n3 = i+6;
        uint64_t n4 = i+8,    n5 = i+10,   n6 = i+12,   n7 = i+14;

        uint32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0;
        uint64_t p0 = n0, p1 = n1, p2 = n2, p3 = n3, p4 = n4, p5 = n5, p6 = n6, p7 = n7;

        while ((n0|n1|n2|n3|n4|n5|n6|n7) >= CACHE_LIMIT) {
            step_hybrid(n0, s0, p0, i,    res.first_overflow);
            step_hybrid(n1, s1, p1, i+2,  res.first_overflow);
            step_hybrid(n2, s2, p2, i+4,  res.first_overflow);
            step_hybrid(n3, s3, p3, i+6,  res.first_overflow);
            step_hybrid(n4, s4, p4, i+8,  res.first_overflow);
            step_hybrid(n5, s5, p5, i+10, res.first_overflow);
            step_hybrid(n6, s6, p6, i+12, res.first_overflow);
            step_hybrid(n7, s7, p7, i+14, res.first_overflow);
        }

        s0 += cache[n0]; s1 += cache[n1]; s2 += cache[n2]; s3 += cache[n3];
        s4 += cache[n4]; s5 += cache[n5]; s6 += cache[n6]; s7 += cache[n7];

        // Update Histogram
        auto h_inc = [&](uint32_t s) { res.histogram[s < HIST_SIZE ? s : HIST_SIZE-1]++; };
        h_inc(s0); h_inc(s1); h_inc(s2); h_inc(s3);
        h_inc(s4); h_inc(s5); h_inc(s6); h_inc(s7);

        // Update Max
        auto check = [&](uint32_t s, uint64_t seed) {
            if (s > res.max_length) { res.max_length = s; res.max_seed = seed; }
        };
        check(s0, i); check(s1, i+2); check(s2, i+4); check(s3, i+6);
        check(s4, i+8); check(s5, i+10); check(s6, i+12); check(s7, i+14);

        uint64_t local_peak = std::max({p0, p1, p2, p3, p4, p5, p6, p7});
        if (local_peak > res.max_peak) res.max_peak = local_peak;
    }

    // Cleanup Remainder
    for (; i <= end; i += 2) {
        uint64_t n = i;
        uint32_t s = 0;
        uint64_t p = n;
        while(n >= CACHE_LIMIT) {
            step_hybrid(n, s, p, i, res.first_overflow);
            if (n == 0) break;  //Signal to stop on overflow
        }
        s += cache[n];
        res.histogram[s < HIST_SIZE ? s : HIST_SIZE-1]++;
        if (s > res.max_length) { res.max_length = s; res.max_seed = i; }
        if (p > res.max_peak) res.max_peak = p;
    }

    // Merge Results with minimal locking
    atomic_update_min(global_first_overflow, res.first_overflow);
    atomic_update_max(global_max_peak, res.max_peak);

    uint32_t cur_len = global_longest_len.load(std::memory_order_relaxed);
    while (res.max_length > cur_len) {
        if (global_longest_len.compare_exchange_weak(cur_len, res.max_length,
                                                     std::memory_order_relaxed)) {
            global_longest_seed.store(res.max_seed, std::memory_order_relaxed);
            break;
        }
    }

    // Only lock for histogram merge (shortest critical section)
    {
        std::lock_guard<std::mutex> lock(histogram_mutex);
        for (size_t j = 0; j < HIST_SIZE; ++j) {
            if (res.histogram[j] > 0) {
                global_histogram_map[j] += res.histogram[j];
            }
        }
    }
    std::ostringstream oss;
    oss << "  ✓ Worker " << thread_id << " finished.\n";
    write_to_log(oss.str());
}
// ================= MAIN =================

std::string format_number(uint64_t num) {
    if (num == (uint64_t)INT64_MAX) return "NONE";
    std::string s = std::to_string(num);
    int insertPosition = s.length() - 3;
    while (insertPosition > 0) { s.insert(insertPosition, ","); insertPosition -= 3; }
    return s;
}

int collatz_compute(uint64_t limit, CollatzResult& out, int countThread) {
    global_first_overflow.store(INT64_MAX);
    global_max_peak.store(0);
    global_longest_seed.store(1);
    global_longest_len.store(0);
    global_histogram_map.clear();
    collatz_cache.clear();
    auto start = std::chrono::high_resolution_clock::now();
    build_cache_parallel();

    //int hw = std::thread::hardware_concurrency();
    if (countThread == 0) countThread = 1;
    int num_threads = countThread;
    if (limit < num_threads) num_threads = static_cast<unsigned int>(limit == 0 ? 1 : limit);

    std::vector<std::thread> threads;
    uint64_t chunk = limit / num_threads;
    if (chunk == 0) chunk = 1;

    for (unsigned int i = 0; i < num_threads; ++i) {
        uint64_t t_start = i * chunk + 1;
        uint64_t t_end = (i == num_threads - 1) ? limit : (i + 1) * chunk;
        if (t_start > limit) break;
        if (t_end > limit) t_end = limit;
        threads.emplace_back(worker_static, t_start, t_end, i);
    }
    for (auto& t: threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();

    CollatzResult r{};
    r.limit = limit;
    r.seconds = seconds;
    r.throughput = seconds > 0 ? (limit / seconds / 1e9) : 0.0;
    r.first_overflow = global_first_overflow.load();
    r.longest_len = global_longest_len.load();
    r.longest_seed = global_longest_seed.load();
    r.max_peak = global_max_peak.load();
    out = r;
    return 0;
}


int collatz_compute_and_write_pipe_impl(int countThread, uint64_t limit, int result_fd, int log_fd) {
    CollatzResult result{};

    global_log_fd.store(log_fd, std::memory_order_relaxed);

    int ret = collatz_compute(limit, result, countThread);

    if (result_fd != -1) {
        ssize_t bytes_written = write(result_fd, &result, sizeof(result));
        if (bytes_written != sizeof(result)) {
            close(result_fd);
            global_log_fd.store(-1, std::memory_order_relaxed);
            return -2;
        }
        close(result_fd);
    }


    close(log_fd);
    global_log_fd.store(-1, std::memory_order_relaxed);

    return 0;
}

extern "C" int collatz_compute_and_write_pipe(int countThread,uint64_t limit, int result_fd, int log_fd)
{
    return collatz_compute_and_write_pipe_impl(countThread,limit, result_fd, log_fd);
}
