#pragma once

#include <print>

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

class VulkanBackend;
class Scene;
void drawDebugUI(VulkanBackend& backend, Scene& scene, double dt);
