#pragma once

#include "engine.h"

auto idx(u16 x, u16 y, u16 w, u16 h, u16 componentCount, u16 component) -> u32
{
    x = (x + w) % w;
    y = (y + h) % h;
    return componentCount * y * w + (componentCount * x + component);
}

auto tangentNormalMapToBumpMap(u8* normal, u16 width, u16 height) -> std::vector<u8>
{
    // TODO: this could be done in a compute shader much faster

    // Assumes data is RGBA, 1B per channel. For the time being this also outputs RGBA
    std::vector<u8> bumpMap;
    bumpMap.resize(width * height * 4 * 1);

    f32* laplacian = new f32[width * height];
    for (u16 i = 0; i < height; ++i)
    {
        for (u16 j = 0; j < width; ++j)
        {
            f32 ddx = (f32)normal[idx(j + 1, i, width, height, 4, 0)] / 255.f - (f32)normal[idx(j - 1, i, width, height, 4, 0)] / 255.f;
            f32 ddy = (f32)normal[idx(j, i + 1, width, height, 4, 1)] / 255.f - (f32)normal[idx(j, i - 1, width, height, 4, 1)] / 255.f;

            laplacian[idx(j, i, width, height, 1, 0)] = (ddx + ddy) / 2.f;
        }
    }

    // Ping-pong buffers
    f32* src = new f32[width * height];
    f32* dst = new f32[width * height];

    for (u16 i = 0; i < height; ++i)
    {
        for (u16 j = 0; j < width; ++j)
        {
            dst[idx(j, i, width, height, 1, 0)] = 0.5f;
        }
    }

    f32 lo = INFINITY;
    f32 hi = -INFINITY;

    // Number of Poisson iterations
    constexpr u16 N = 500;
    for (u16 t = 0; t < N; ++t) {
        // Swap buffers
        f32* tmp = src;
        src = dst;
        dst = tmp;

        for (u16 i = 0; i < height; ++i)
        {
            for (u16 j = 0; j < width; ++j)
            {
                f32 value = src[idx(j - 1, i, width, height, 1, 0)] + src[idx(j, i - 1, width, height, 1, 0)] +
                            src[idx(j + 1, i, width, height, 1, 0)] + src[idx(j, i + 1, width, height, 1, 0)] +
                            laplacian[idx(j, i, width, height, 1, 0)];
                value *= 1.f / 4.f;
                dst[idx(j, i, width, height, 1, 0)] = value;

                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
        }
    }

    for (u16 i = 0; i < height; ++i)
    {
        for (u16 j = 0; j < width; ++j)
        {
            f32 value = dst[idx(j, i, width, height, 1, 0)];
            value = (value - lo) / (hi - lo);
            dst[idx(j, i, width, height, 1, 0)] = value;
        }
    }

    for (u16 i = 0; i < height; ++i)
    {
        for (u16 j = 0; j < width; ++j)
        {
            f32 value = dst[idx(j, i, width, height, 1, 0)];
            value = (value - lo) / (hi - lo);
            value *= 255.f;

            bumpMap[idx(j, i, width, height, 4, 0)] = value;
            bumpMap[idx(j, i, width, height, 4, 1)] = value;
            bumpMap[idx(j, i, width, height, 4, 2)] = value;
            bumpMap[idx(j, i, width, height, 4, 3)] = 255;
        }
    }

    delete[] laplacian;
    delete[] src;
    delete[] dst;

    return bumpMap;
}