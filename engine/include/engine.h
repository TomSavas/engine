#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include <stdint.h>

#if defined(DEBUG)
    #if defined(__linux__)
        #include <signal.h>

        #define BREAKPOINT raise(SIGTRAP)
    #else
        // Other platforms not supported
        #define BREAKPOINT

    #endif //defined(__linux__)
#else
    #define BREAKPOINT
#endif // defined(DEBUG)

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;