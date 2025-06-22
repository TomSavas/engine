#pragma once

#include "engine.h"

#define ZoneScopedCpuGpuAuto(name, currentFrame) \
    ZoneScopedCpuGpu(currentFrame.tracyCtx, currentFrame.tracyCmdBuffer, name)
#define ZoneScopedCpuGpu(ctx, cmd, name)   \
    do                                     \
    {                                      \
        ZoneScopedN(name " CPU");          \
        TracyVkZone(ctx, cmd, name " GPU") \
    } while (0)

#include <iostream>
#define VK_CHECK(x)                                            \
    do                                                         \
    {                                                          \
        VkResult err = x;                                      \
        if (err)                                               \
        {                                                      \
            std::cout << "Vulkan error: " << err << std::endl; \
            BREAKPOINT;                                        \
        }                                                      \
    } while (0)
