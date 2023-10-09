#pragma once

#include "CDTime.hpp"
#include "libzplay.h"

#include <cstdint>
#include <string>
#include <map>

using namespace libZPlay;

struct CDTrack
{
    std::string path;
    TStreamFormat format;
    CDTime length;
    CDTime position;
};

class CDTrackList
{
public:
    CDTrackList(
        const std::string& path, const std::string& prefix, ZPlay* player);
    const std::map<int32_t, const CDTrack>& map();
    const CDTrack& get(int32_t index);
    const CDTrack& last();
    bool isValid(int32_t index);
    bool isAudio(int32_t index);
    void toTrackTime(CDTime& time);

private:
    std::map<int32_t, const CDTrack> m_tracks;
    CDTrack m_invalidTrack;
};