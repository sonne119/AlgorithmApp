#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#ifdef _WIN32
    // Windows
    #include <io.h>
    #include <windows.h>
    #include <intrin.h>
    #include <fcntl.h>
    
    // POSIX types
    #ifndef ssize_t
        #ifdef _WIN64
            typedef __int64 ssize_t;
        #else
            typedef long ssize_t;
        #endif
    #endif
    
    // Map POSIX to Windows
    #define write _write
    #define read _read
    #define close _close
    #define fileno _fileno
    #define isatty _isatty
    #define getpid _getpid
    
    typedef int pid_t;
    
    #define NO_INT128
        
#else
    // Unix/macOS
    #include <unistd.h>
    #include <sys/types.h>
#endif

#endif // PLATFORM_COMPAT_H

