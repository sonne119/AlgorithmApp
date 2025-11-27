#include "CollatzRunner.h"
#include <iostream>
#include <thread>
//#include <unistd.h>
#include <cstring>
#include "../lib/collatz.h"
#include "../lib/platform_compat.h"


#ifdef _WIN32
#include <fcntl.h>
// Windows pipe implementation
inline int pipe(int fds[2]) {
    return _pipe(fds, 4096, _O_BINARY);
}
#endif

CollatzRunner::CollatzRunner() {
}

CollatzRunner::~CollatzRunner() {
}

CollatzResult CollatzRunner::Compute(LogCallback logCallback)
{
    int result_fds[2], log_fds[2];
    if (pipe(result_fds) != 0 || pipe(log_fds) != 0) {
        return CollatzResult{};
    }

    std::thread log_thread([count = this->threadCount, result_write = result_fds[1], log_write = log_fds[1], lim = this->limit] {
        collatz_compute_and_write_pipe(count, lim, result_write, log_write);
    });

    std::thread worker_thread([log_read = log_fds[0], logCallback]() {
        char buf[4096];
        ssize_t n;
        while ((n = read(log_read, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            std::string message(buf);

            if (logCallback) {
                logCallback(message);
            }
        }
        close(log_read);
    });

    // Read result
    CollatzResult r{};
    ssize_t bytes_read = read(result_fds[0], &r, sizeof(r));
    (void)bytes_read;
    close(result_fds[0]);

    log_thread.join();
    worker_thread.join();

    return r;
}
