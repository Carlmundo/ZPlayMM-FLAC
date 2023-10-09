#pragma once

#include <cstdint>
#include <map>

class CDTime
{
public:
    int32_t track;
    int32_t seconds;

    void fromMciTime(int32_t mciTime, int32_t format);
    int32_t toMciTime(int32_t format);
};