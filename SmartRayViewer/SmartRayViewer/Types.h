#pragma once
#include <cstdint>
#include <vector>
#include <memory>

#include "SR_API_public.h"

struct PcFrame
{
    int camIndex = 0;
    uint64_t frameNo = 0;

    uint32_t numPoints = 0;
    uint32_t numProfiles = 0;

    std::vector<SR_3DPOINT> points;
    std::vector<unsigned short> intensity;
};

struct ZMapFrame
{
    int camIndex = 0;
    uint64_t frameNo = 0;

    int w = 0;
    int h = 0;
    uint16_t invalid = 0;

    std::vector<uint16_t> z; // w*h
};

struct ThicknessFrame
{
    uint64_t topFrameNo = 0;
    uint64_t bottomFrameNo = 0;
    std::vector<double> t;
};
