#pragma once

int idx(int x, int y, int w, int h, int componentCount, int component)
{
    x = (x + w) % w;
    y = (y + h) % h;
    return componentCount * y * w + (componentCount * x + component);
}

std::vector<uint8_t> tangentNormalMapToBumpMap(uint8_t* normal, uint32_t width, uint32_t height)
{
    // TODO: this could be done in a compute shader much faster

    // Assumes data is RGBA, 1B per channel. For the time being this also outputs RGBA
    std::vector<uint8_t> bumpMap;
    bumpMap.resize(width * height * 4 * 1);

    float* laplacian = new float[width * height];
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            float ddx = (float)normal[idx(j + 1, i, width, height, 4, 0)] / 255.f - (float)normal[idx(j - 1, i, width, height, 4, 0)] / 255.f;
            float ddy = (float)normal[idx(j, i + 1, width, height, 4, 1)] / 255.f - (float)normal[idx(j, i - 1, width, height, 4, 1)] / 255.f;

            laplacian[idx(j, i, width, height, 1, 0)] = (ddx + ddy) / 2.f;
        }
    }

    // Ping-pong buffers
    float* src = new float[width * height];
    float* dst = new float[width * height];

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            dst[idx(j, i, width, height, 1, 0)] = 0.5f;
        }
    }

    float lo = INFINITY;
    float hi = -INFINITY;

    // Number of Poisson iterations
    constexpr int N = 500;
    for (int t = 0; t < N; ++t) {
        // Swap buffers
        float* tmp = src;
        src = dst;
        dst = tmp;

        for (int i = 0; i < height; ++i)
        {
            for (int j = 0; j < width; ++j)
            {
                float value = src[idx(j - 1, i, width, height, 1, 0)] + src[idx(j, i - 1, width, height, 1, 0)] +
                              src[idx(j + 1, i, width, height, 1, 0)] + src[idx(j, i + 1, width, height, 1, 0)] +
                              laplacian[idx(j, i, width, height, 1, 0)];
                value *= 1.f / 4.f;
                dst[idx(j, i, width, height, 1, 0)] = value;

                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
        }
    }

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            float value = dst[idx(j, i, width, height, 1, 0)];
            value = (value - lo) / (hi - lo);
            dst[idx(j, i, width, height, 1, 0)] = value;
        }
    }

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            float value = dst[idx(j, i, width, height, 1, 0)];
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