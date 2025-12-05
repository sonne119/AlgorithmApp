#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <climits>
#include <algorithm>
#include <iomanip>
#include "../../../../Algorithm_windows/CollatzApp_Windows_Fixed/lib/platform_compat.h"
#include "../../../../Algorithm_windows/CollatzApp_Windows_Fixed/lib/collatz_simd.h"

static std::atomic<int> global_simd__log_fd{-1};

// --- PLATFORM & SIMD DETECTION ---
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(_MSC_VER)
#include <intrin.h>
#include <immintrin.h>
#pragma intrinsic(_BitScanForward64)
inline int ctz64_msvc(uint64_t n) {
    unsigned long index;
    _BitScanForward64(&index, n);
    return static_cast<int>(index);
}
#define CTZ(n) ctz64_msvc(n)
#else
#include <immintrin.h>
#define CTZ(n) __builtin_ctzll(n)
#endif
#define IS_X86
#elif defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define IS_ARM
#define CTZ(n) __builtin_ctzll(n)
#else
#define CTZ(n) 0
#endif

// ================= HELPER ========================
// write to log pipe
static void write_to_log_simd(const std::string& message) {
    int log_fd = global_simd__log_fd.load(std::memory_order_relaxed);
    if (log_fd != -1) {
        write(log_fd, message.c_str(), message.length());
    }
    std::cout << message << std::flush;
}

// --- CONFIGURATION ---
constexpr uint64_t CACHE_LIMIT = 1ULL << 27;
// Match SAFE_THRESHOLD from collatz.cpp: (INT64_MAX - 1) / 3
constexpr uint64_t OVERFLOW_THRESHOLD = 3074457345618258602ULL;

// Global Data
static std::vector<uint16_t> collatz_cache;

// Global Atomics
std::atomic<uint64_t> g_max_peak(0);
std::atomic<uint64_t> g_longest_seed(0);
std::atomic<uint32_t> g_longest_len(0);
std::atomic<uint64_t> g_first_overflow(UINT64_MAX);

// --- ATOMIC UPDATES ---
void atomic_update_max_peak(uint64_t val) {
    uint64_t prev = g_max_peak.load(std::memory_order_relaxed);
    while (prev < val && !g_max_peak.compare_exchange_weak(prev, val, std::memory_order_relaxed));
}

void atomic_update_longest(uint32_t len, uint64_t seed) {
    uint32_t prev = g_longest_len.load(std::memory_order_relaxed);
    while (prev < len) {
        if (g_longest_len.compare_exchange_weak(prev, len, std::memory_order_relaxed)) {
            g_longest_seed.store(seed, std::memory_order_relaxed);
            break;
        }
    }
}

void atomic_update_overflow(uint64_t seed) {
    uint64_t prev = g_first_overflow.load(std::memory_order_relaxed);
    while (prev > seed && !g_first_overflow.compare_exchange_weak(prev, seed, std::memory_order_relaxed));
}

// --- BUILD CACHE ---
void build_cache() {

    write_to_log_simd("  > Building Cache simd ... ");
    auto start = std::chrono::high_resolution_clock::now();

    collatz_cache.resize(CACHE_LIMIT);
    collatz_cache[1] = 0;

    for (uint64_t i = 2; i < CACHE_LIMIT; ++i) {
        uint64_t n = i;
        uint16_t steps = 0;
        while (n >= i) {
            if ((n & 1) == 0) {
                n >>= 1;
                steps++;
            } else {
                n = (n * 3 + 1) >> 1;
                steps += 2;
            }
        }
        collatz_cache[i] = steps + collatz_cache[n];
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::ostringstream oss;
    oss << "Done (" << std::chrono::duration<double>(end - start).count() << "s)\n";
    write_to_log_simd(oss.str());
}

// --- WORKER ARM ROUTINE ---
#ifdef IS_ARM
void worker_simd(uint64_t start, uint64_t end, int thread_id) {
    uint64_t local_max_peak = 0;
    uint32_t local_longest_len = 0;
    uint64_t local_longest_seed = 0;
    uint64_t local_first_overflow = UINT64_MAX;
    const uint16_t* cache = collatz_cache.data();

    const uint64x2_t v_limit = vdupq_n_u64(CACHE_LIMIT);
    const uint64x2_t v_one   = vdupq_n_u64(1);
    const uint64x2_t v_two   = vdupq_n_u64(2);
    const uint64x2_t v_thresh= vdupq_n_u64(OVERFLOW_THRESHOLD);
    const uint64x2_t v_zero  = vdupq_n_u64(0);

    uint64_t i = start;
    if ((i & 1) == 0) i++;

    // 16 numbers (8 vectors of 2)
    for (; i + 30 < end; i += 32) {
        uint64_t seeds[16] = {
            i, i+2, i+4, i+6, i+8, i+10, i+12, i+14,
            i+16, i+18, i+20, i+22, i+24, i+26, i+28, i+30
        };

        uint64x2_t v0 = vld1q_u64(&seeds[0]); uint64x2_t v1 = vld1q_u64(&seeds[2]);
        uint64x2_t v2 = vld1q_u64(&seeds[4]); uint64x2_t v3 = vld1q_u64(&seeds[6]);
        uint64x2_t v4 = vld1q_u64(&seeds[8]); uint64x2_t v5 = vld1q_u64(&seeds[10]);
        uint64x2_t v6 = vld1q_u64(&seeds[12]); uint64x2_t v7 = vld1q_u64(&seeds[14]);

        uint64x2_t s0 = v_zero; uint64x2_t s1 = v_zero; uint64x2_t s2 = v_zero; uint64x2_t s3 = v_zero;
        uint64x2_t s4 = v_zero; uint64x2_t s5 = v_zero; uint64x2_t s6 = v_zero; uint64x2_t s7 = v_zero;

        uint64x2_t sd0 = v0; uint64x2_t sd1 = v1; uint64x2_t sd2 = v2; uint64x2_t sd3 = v3;
        uint64x2_t sd4 = v4; uint64x2_t sd5 = v5; uint64x2_t sd6 = v6; uint64x2_t sd7 = v7;

        uint64x2_t p0 = v0; uint64x2_t p1 = v1; uint64x2_t p2 = v2; uint64x2_t p3 = v3;
        uint64x2_t p4 = v4; uint64x2_t p5 = v5; uint64x2_t p6 = v6; uint64x2_t p7 = v7;

        uint64x2_t m0 = vcgtq_u64(v0, v_limit); uint64x2_t m1 = vcgtq_u64(v1, v_limit);
        uint64x2_t m2 = vcgtq_u64(v2, v_limit); uint64x2_t m3 = vcgtq_u64(v3, v_limit);
        uint64x2_t m4 = vcgtq_u64(v4, v_limit); uint64x2_t m5 = vcgtq_u64(v5, v_limit);
        uint64x2_t m6 = vcgtq_u64(v6, v_limit); uint64x2_t m7 = vcgtq_u64(v7, v_limit);

        uint64x2_t ovf = v_zero;

        while (1) {
            uint64x2_t any = vorrq_u64(vorrq_u64(vorrq_u64(m0,m1), vorrq_u64(m2,m3)),
                                       vorrq_u64(vorrq_u64(m4,m5), vorrq_u64(m6,m7)));
            if (vgetq_lane_u64(any, 0) == 0 && vgetq_lane_u64(any, 1) == 0) break;

#define STEP_NEON(V, S, M, P) \
            if ((vgetq_lane_u64(M, 0) | vgetq_lane_u64(M, 1)) != 0) { \
                    uint64x2_t new_peak = vandq_u64(vcgtq_u64(V, P), M); \
                    P = vbslq_u64(new_peak, V, P); \
                    uint64x2_t is_odd = vtstq_u64(V, v_one); \
                    uint64x2_t v_even = vshrq_n_u64(V, 1); \
                    uint64x2_t v_odd = vaddq_u64(vaddq_u64(V, v_even), v_one); \
                    uint64x2_t is_ovf = vandq_u64(is_odd, vcgtq_u64(V, v_thresh)); \
                    ovf = vorrq_u64(ovf, vandq_u64(is_ovf, M)); \
                    uint64x2_t next = vbslq_u64(is_odd, v_odd, v_even); \
                    uint64x2_t inc = vbslq_u64(is_odd, v_two, v_one); \
                    V = vbslq_u64(M, next, V); \
                    S = vaddq_u64(S, vandq_u64(inc, M)); \
                    M = vcgtq_u64(V, v_limit); \
            }

            STEP_NEON(v0, s0, m0, p0); STEP_NEON(v1, s1, m1, p1);
            STEP_NEON(v2, s2, m2, p2); STEP_NEON(v3, s3, m3, p3);
            STEP_NEON(v4, s4, m4, p4); STEP_NEON(v5, s5, m5, p5);
            STEP_NEON(v6, s6, m6, p6); STEP_NEON(v7, s7, m7, p7);
        }

        auto finalize = [&](uint64x2_t val, uint64x2_t st, uint64x2_t sd, uint64x2_t pk) {
            uint64_t v[2], s[2], d[2], p[2];
            vst1q_u64(v, val); vst1q_u64(s, st); vst1q_u64(d, sd); vst1q_u64(p, pk);
            for(int k=0; k<2; k++) {
                uint64_t n = v[k];
                while (n >= CACHE_LIMIT) {
                    if (n > p[k]) p[k] = n;
                    if (n & 1) { n = (n * 3 + 1) >> 1; s[k] += 2; }
                    else { n >>= 1; s[k]++; }
                }
                s[k] += cache[n];
                if (s[k] > local_longest_len) { local_longest_len = s[k]; local_longest_seed = d[k]; }
                if (p[k] > local_max_peak) local_max_peak = p[k];
            }
        };

        finalize(v0, s0, sd0, p0); finalize(v1, s1, sd1, p1);
        finalize(v2, s2, sd2, p2); finalize(v3, s3, sd3, p3);
        finalize(v4, s4, sd4, p4); finalize(v5, s5, sd5, p5);
        finalize(v6, s6, sd6, p6); finalize(v7, s7, sd7, p7);

        if ((vgetq_lane_u64(ovf, 0) | vgetq_lane_u64(ovf, 1)) != 0) {
            // Re-check seeds scalar-wise to find exact overflow
            for(int k=0; k<16; k++) {
                uint64_t n = seeds[k];
                while (n >= CACHE_LIMIT) {
                    if ((n & 1) == 0) {
                        n >>= 1;
                    } else {
                        if (n > OVERFLOW_THRESHOLD) {
                            if (seeds[k] < local_first_overflow) local_first_overflow = seeds[k];
                            break;
                        }
                        n = (n * 3 + 1) >> 1;
                    }
                }
            }
        }
    }

    // Scalar Cleanup
    for (; i < end; i += 2) {
        uint64_t n = i; uint64_t peak = n; uint32_t steps = 0; bool overflowed = false;
        while (n >= CACHE_LIMIT) {
            if (n > peak) peak = n;
            if ((n & 1) == 0) { int z = CTZ(n); n >>= z; steps += z; }
            else {
                if (n > OVERFLOW_THRESHOLD) { if (local_first_overflow > i) local_first_overflow = i; overflowed = true; break; }
                n = (n * 3 + 1) >> 1; steps += 2;
            }
        }
        if (!overflowed) {
            steps += cache[n];
            if (steps > local_longest_len) { local_longest_len = steps; local_longest_seed = i; }
            if (peak > local_max_peak) local_max_peak = peak;
        }
    }
    atomic_update_max_peak(local_max_peak);
    atomic_update_longest(local_longest_len, local_longest_seed);
    atomic_update_overflow(local_first_overflow);

    std::ostringstream oss;
    oss << "  ✓ Worker simd " << thread_id << " finished.\n";
    write_to_log_simd(oss.str());

}
#endif

// --- WORKER WINDOWS / LINUX x86 ---
#ifdef IS_X86
void worker_simd(uint64_t start, uint64_t end, int thread_id ) {
    uint64_t local_max_peak = 0;
    uint32_t local_longest_len = 0;
    uint64_t local_longest_seed = 0;
    uint64_t local_first_overflow = UINT64_MAX;
    const uint16_t* cache = collatz_cache.data();

    // AVX2 Constants
    const __m256i v_limit  = _mm256_set1_epi64x(CACHE_LIMIT);
    const __m256i v_one    = _mm256_set1_epi64x(1);
    const __m256i v_two    = _mm256_set1_epi64x(2);
    const __m256i v_thresh = _mm256_set1_epi64x(OVERFLOW_THRESHOLD);
    const __m256i v_sign_flip = _mm256_set1_epi64x(0x8000000000000000ULL);

    uint64_t i = start;
    if ((i & 1) == 0) i++;

    // AVX2 Unroll: 16 numbers (4 vectors of 4)
    for (; i + 30 < end; i += 32) {
        // Load seeds [i...i+30] into 4 vectors
        __m256i v0 = _mm256_set_epi64x(i+6, i+4, i+2, i);
        __m256i v1 = _mm256_set_epi64x(i+14, i+12, i+10, i+8);
        __m256i v2 = _mm256_set_epi64x(i+22, i+20, i+18, i+16);
        __m256i v3 = _mm256_set_epi64x(i+30, i+28, i+26, i+24);

        __m256i s0 = _mm256_setzero_si256(); __m256i s1 = _mm256_setzero_si256();
        __m256i s2 = _mm256_setzero_si256(); __m256i s3 = _mm256_setzero_si256();

        __m256i sd0 = v0; __m256i sd1 = v1; __m256i sd2 = v2; __m256i sd3 = v3;
        __m256i p0 = v0;  __m256i p1 = v1;  __m256i p2 = v2;  __m256i p3 = v3;

        // Unsigned comparison: flip sign bit for proper comparison
        auto cmpgt_u64 = [](__m256i a, __m256i b, __m256i flip) {
            return _mm256_cmpgt_epi64(_mm256_xor_si256(a, flip), _mm256_xor_si256(b, flip));
        };

        __m256i m0 = cmpgt_u64(v0, v_limit, v_sign_flip);
        __m256i m1 = cmpgt_u64(v1, v_limit, v_sign_flip);
        __m256i m2 = cmpgt_u64(v2, v_limit, v_sign_flip);
        __m256i m3 = cmpgt_u64(v3, v_limit, v_sign_flip);

        __m256i ovf0 = _mm256_setzero_si256();
        __m256i ovf1 = _mm256_setzero_si256();
        __m256i ovf2 = _mm256_setzero_si256();
        __m256i ovf3 = _mm256_setzero_si256();

        while (1) {
            int active = !_mm256_testz_si256(m0, m0) || !_mm256_testz_si256(m1, m1) ||
                         !_mm256_testz_si256(m2, m2) || !_mm256_testz_si256(m3, m3);
            if (!active) break;

#define STEP_AVX(V, S, M, P, OVF) \
            if (!_mm256_testz_si256(M, M)) { \
                    /* Update Peak */ \
                    __m256i gt = cmpgt_u64(V, P, v_sign_flip); \
                    __m256i upd = _mm256_and_si256(gt, M); \
                    P = _mm256_blendv_epi8(P, V, upd); \
                    \
                    /* Math */ \
                    __m256i v_even = _mm256_srli_epi64(V, 1); \
                    __m256i v_odd  = _mm256_add_epi64(V, v_even); \
                    v_odd = _mm256_add_epi64(v_odd, v_one); \
                    \
                    /* Is Odd? */ \
                    __m256i is_odd = _mm256_srai_epi64(_mm256_slli_epi64(V, 63), 63); \
                    \
                    /* Overflow Detection */ \
                    __m256i is_ovf_mask = _mm256_and_si256(is_odd, cmpgt_u64(V, v_thresh, v_sign_flip)); \
                    OVF = _mm256_or_si256(OVF, _mm256_and_si256(is_ovf_mask, M)); \
                    \
                    /* Next */ \
                    __m256i next = _mm256_blendv_epi8(v_even, v_odd, is_odd); \
                    __m256i inc  = _mm256_blendv_epi8(v_one, v_two, is_odd); \
                    \
                    /* Update Active */ \
                    V = _mm256_blendv_epi8(V, next, M); \
                    S = _mm256_add_epi64(S, _mm256_and_si256(inc, M)); \
                    M = cmpgt_u64(V, v_limit, v_sign_flip); \
            }

            STEP_AVX(v0, s0, m0, p0, ovf0);
            STEP_AVX(v1, s1, m1, p1, ovf1);
            STEP_AVX(v2, s2, m2, p2, ovf2);
            STEP_AVX(v3, s3, m3, p3, ovf3);
        }

        // Check for overflow
        __m256i ovf_all = _mm256_or_si256(_mm256_or_si256(ovf0, ovf1), _mm256_or_si256(ovf2, ovf3));
        if (!_mm256_testz_si256(ovf_all, ovf_all)) {
            uint64_t o[4][4];
            _mm256_storeu_si256((__m256i*)o[0], ovf0);
            _mm256_storeu_si256((__m256i*)o[1], ovf1);
            _mm256_storeu_si256((__m256i*)o[2], ovf2);
            _mm256_storeu_si256((__m256i*)o[3], ovf3);

            for(int v=0; v<4; v++) {
                for(int l=0; l<4; l++) {
                    if (o[v][l]) {
                        uint64_t seed = i + (v * 8) + (l * 2);
                        if (seed < local_first_overflow) local_first_overflow = seed;
                    }
                }
            }
        }

        // Finalize
        auto finalize = [&](__m256i val, __m256i st, __m256i sd, __m256i pk) {
            uint64_t v[4], s[4], d[4], p[4];
            _mm256_storeu_si256((__m256i*)v, val); _mm256_storeu_si256((__m256i*)s, st);
            _mm256_storeu_si256((__m256i*)d, sd);  _mm256_storeu_si256((__m256i*)p, pk);

            for(int k=0; k<4; k++) {
                uint64_t n = v[k];
                while (n >= CACHE_LIMIT) {
                    if (n > p[k]) p[k] = n;
                    if (n & 1) { n = (n * 3 + 1) >> 1; s[k] += 2; }
                    else { n >>= 1; s[k]++; }
                }
                s[k] += cache[n];
                if (s[k] > local_longest_len) { local_longest_len = s[k]; local_longest_seed = d[k]; }
                if (p[k] > local_max_peak) local_max_peak = p[k];
            }
        };

        finalize(v0, s0, sd0, p0); finalize(v1, s1, sd1, p1);
        finalize(v2, s2, sd2, p2); finalize(v3, s3, sd3, p3);
    }

    // Scalar Cleanup
    for (; i < end; i += 2) {
        uint64_t n = i; uint64_t peak = n; uint32_t steps = 0; bool overflowed = false;
        while (n >= CACHE_LIMIT) {
            if (n > peak) peak = n;
            if ((n & 1) == 0) { int z = CTZ(n); n >>= z; steps += z; }
            else {
                if (n > OVERFLOW_THRESHOLD) { if (local_first_overflow > i) local_first_overflow = i; overflowed = true; break; }
                n = (n * 3 + 1) >> 1; steps += 2;
            }
        }
        if (!overflowed) {
            steps += cache[n];
            if (steps > local_longest_len) { local_longest_len = steps; local_longest_seed = i; }
            if (peak > local_max_peak) local_max_peak = peak;
        }
    }
    atomic_update_max_peak(local_max_peak);
    atomic_update_longest(local_longest_len, local_longest_seed);
    atomic_update_overflow(local_first_overflow);
    std::ostringstream oss;
    oss << "  ✓ Worker_simd " << thread_id << " finished.\n";
    write_to_log(oss.str());
}
#endif

// --- MAIN ---
int collatz_compute_simd(uint64_t limit, CollatzResult& out, int countThread) {
    // Reset global atomics
    g_first_overflow.store(UINT64_MAX, std::memory_order_relaxed);
    g_max_peak.store(0, std::memory_order_relaxed);
    g_longest_seed.store(1, std::memory_order_relaxed);
    g_longest_len.store(0, std::memory_order_relaxed);

    build_cache();

    unsigned int num_threads = (countThread > 0) ? countThread : std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::cout << "Calculating 1.." << limit << " with " << num_threads << " threads." << std::endl;

#ifdef IS_ARM
    std::cout << "Apple Silicon" << std::endl;
#elif defined(IS_X86)
    std::cout << "Windows/Intel AVX2" << std::endl;
#else
    std::cout << "Scalar Fallback" << std::endl;
#endif

    std::vector<std::thread> threads;
    uint64_t chunk = limit / num_threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        uint64_t s = (i == 0) ? 1 : i * chunk;
        uint64_t e = (i == num_threads - 1) ? limit : s + chunk;
        threads.emplace_back(worker_simd, s, e, i);
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    out.limit = limit;
    out.seconds = elapsed.count();
    out.throughput = limit / elapsed.count();
    out.first_overflow = g_first_overflow.load(std::memory_order_acquire);
    if (out.first_overflow == UINT64_MAX) out.first_overflow = 0;
    out.longest_len = g_longest_len.load(std::memory_order_acquire);
    out.longest_seed = g_longest_seed.load(std::memory_order_acquire);
    out.max_peak = g_max_peak.load(std::memory_order_acquire);

    return 0;
}

int collatz_compute_simd__and_write_pipe_impl(int countThread, uint64_t limit, int result_fd, int log_fd) {
    CollatzResult result{};

    global_simd__log_fd.store(log_fd, std::memory_order_relaxed);

    int ret = collatz_compute_simd(limit, result, countThread);

    if (result_fd != -1) {
        ssize_t bytes_written = write(result_fd, &result, sizeof(result));
        if (bytes_written != sizeof(result)) {
            close(result_fd);
            global_simd__log_fd.store(-1, std::memory_order_relaxed);
            return -2;
        }
        close(result_fd);
    }

    if (log_fd != -1) {
        close(log_fd);
    }
    global_simd__log_fd.store(-1, std::memory_order_relaxed);

    return ret;
}

extern "C" int collatz_compute_simd_and_write_pipe(int countThread, uint64_t limit, int result_fd, int log_fd)
{
    return collatz_compute_simd__and_write_pipe_impl(countThread, limit, result_fd, log_fd);
}
