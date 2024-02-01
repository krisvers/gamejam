#ifndef PLATFORMS_HPP
#define PLATFORMS_HPP

#if defined(_WIN32) || defined(__WIN32__) || defined(_MSC_VER)
#define PLATFORM_WINDOWS 1
#define PLATFORM_WIN32 1
#endif

#if defined(_WIN64)
#ifndef PLATFORM_WINDOWS
#define PLATFORM_WINDOWS 1
#endif
#define PLATFORM_WIN64 1
#endif

#if defined(__linux__)
#define PLATFORM_LINUX 1
#endif

#if defined(__unix__)
#define PLATFORM_UNIX 1
#endif

#if defined(__posix__)
#define PLATFORM_POSIX 1
#endif

#if defined(__APPLE__)
#define PLATFORM_APPLE 1
#include <TargetConditionals.h>

#if defined(TARGET_OS_IPHONE)
#define PLATFORM_APPLE_IPHONE
#endif

#if defined(TARGET_OS_MAC)
#define PLATFORM_APPLE_MACOS
#endif

#if defined(TARGET_OS_UNIX)
#define PLATFORM_APPLE_UNIX
#endif

#if defined(TARGET_OS_EMBEDDED)
#define PLATFORM_APPLE_EMBEDDED
#endif
#endif

/* compiler macros */
#if defined(_MSC_VER)
#define PLATFORM_MSVC 1
#endif

#if defined(__GNUC__)
#define PLATFORM_GCC
#endif

#if defined(__clang__)
#define PLATFORM_CLANG
#endif

#if defined(__MINGW32__)
#define PLATFORM_MINGW 1
#define PLATFORM_MINGW32 1
#endif

#if defined(__MINGW64__)
#ifndef PLATFORM_MINGW
#define PLATFORM_MINGW 1
#endif
#define PLATFORM_MINGW32 1
#endif

#if defined(__BORLANDC__)
#define PLATFORM_BORLANDC 1
#endif

#endif
