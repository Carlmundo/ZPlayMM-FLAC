#include "CDTrackList.hpp"
#include "Logger.hpp"
#include "WinMMError.hpp"

#include <algorithm>

CDTrackList::CDTrackList(
    const std::string& path, const std::string& prefix, ZPlay* player)
    : m_invalidTrack({})
{
    LOG_TRACE("Finding tracks");

    // get module path
    std::string modulePath;
    modulePath.resize(MAX_PATH);
    DWORD size = GetModuleFileName(
        GetModuleHandle(NULL), &modulePath[0], modulePath.capacity());
    modulePath.resize(size);

    // remove module file from path
    modulePath = modulePath.substr(0, modulePath.find_last_of('\\'));

    // find files in music directory
    std::string basePath = std::string(modulePath) + '\\' + path + '\\';
    std::string findPath = basePath + "*";

    WIN32_FIND_DATA fdata;
    HANDLE hFind = FindFirstFile(findPath.c_str(), &fdata);

    if (hFind == INVALID_HANDLE_VALUE) {
        throw WinMMError(
            "Music directory not found: " + basePath, MCIERR_HARDWARE);
    }

    int32_t numTracks = 0;
    std::map<int32_t, CDTrack> tracks;

    do {
        // skip directories
        if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        std::string fileName = std::string(fdata.cFileName);

        // skip files with wrong prefix
        if (fileName.substr(0, prefix.length()) != prefix) {
            continue;
        }

        std::string trackNumberStr = fileName.substr(5, 2);
        int32_t trackNumber;

        try {
            trackNumber = std::stoi(trackNumberStr);
        } catch (const std::exception&) {
            // not a valid track number
            continue;
        }

        // skip track numbers that are out of range
        if (trackNumber <= 0) {
            continue;
        }

        CDTrack track = {};
        track.path = basePath + fileName;
        track.format = player->GetFileFormat(track.path.c_str());

        // skip files with unknown formats
        if (track.format == sfUnknown) {
            continue;
        }

        // open file
        if (!player->OpenFile(track.path.c_str(), track.format)) {
            throw WinMMError(player->GetError(), MCIERR_HARDWARE);
            continue;
        }

        // get track length
        TStreamInfo info;
        player->GetStreamInfo(&info);

        track.length.seconds = info.Length.sec;

        // close file
        if (!player->Close()) {
            throw WinMMError(player->GetError(), MCIERR_HARDWARE);
            continue;
        }

        TStreamHMSTime& time = info.Length.hms;
        LOG_TRACE("%s: %02d:%02d:%02d.%03d", fileName.c_str(),
            time.hour, time.minute, time.second, time.millisecond);

        // put track into map
        tracks[trackNumber] = track;

        // update number of tracks
        numTracks = (std::max)(numTracks, trackNumber);
    } while (FindNextFile(hFind, &fdata) != 0);

    FindClose(hFind);
    hFind = INVALID_HANDLE_VALUE;

    // data tracks are 2 seconds of silence
    CDTrack dataTrack = {};
    dataTrack.length.seconds = 2;

    CDTime position = {};

    for (int32_t i = 1; i < numTracks + 1; i++) {
        // insert data tracks into gaps
        if (tracks[i].path.empty()) {
            tracks[i] = dataTrack;
        }

        CDTrack& track = tracks[i];

        position.track = i;
        track.position = position;
        position.seconds += track.length.seconds;
    }

    // copy temporary map to member map (CDTrack to const CDTrack)
    for (auto trackPair : tracks) {
        m_tracks.insert(trackPair);
    }

    LOG_TRACE("Found %d tracks", numTracks);
}

const std::map<int32_t, const CDTrack>& CDTrackList::map()
{
    return m_tracks;
}

const CDTrack& CDTrackList::get(int32_t index)
{
    if (isValid(index)) {
        return m_tracks[index];
    } else {
        // prevent adding new tracks to the map if the index is invalid
        return m_invalidTrack;
    }
}

const CDTrack& CDTrackList::last()
{
    return m_tracks.rbegin()->second;
}

bool CDTrackList::isValid(int32_t index)
{
    return m_tracks.find(index) != m_tracks.end();
}

bool CDTrackList::isAudio(int32_t index)
{
    return isValid(index) && !m_tracks[index].path.empty();
}

void CDTrackList::toTrackTime(CDTime& time)
{
    for (auto trackPair : m_tracks) {
        const CDTrack& track = trackPair.second;
        if (track.position.seconds >= time.seconds) {
            time.track = trackPair.first;
            time.seconds = track.position.seconds - time.seconds;
            return;
        }
    }

    // offset is out of range, set length to last track length
    auto trackPair = m_tracks.rbegin();
    time.track = trackPair->first;
    time.seconds = trackPair->second.length.seconds;
}