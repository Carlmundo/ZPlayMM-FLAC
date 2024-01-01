#pragma once

#include "CDTime.hpp"
#include "CDTrackList.hpp"
#include "libzplay.h"

#include <Windows.h>
#include <cstdint>
#include <string>

using namespace libZPlay;

class CDPlayer
{
public:
    // initialization
    CDPlayer();
    ~CDPlayer();

    // player status
    bool isOpen();
    bool isPlaying();
    bool isPaused();

    // track status
    int32_t getNumTracks();
    int32_t getCurrentTrack();
    int32_t getLength(int32_t index);
    int32_t getPosition(int32_t index);
    int32_t getType(int32_t index);

    // properties
    int32_t getDeviceID();
    void setTimeFormat(int32_t timeFormat);
    int32_t getTimeFormat();
    void setVolume(int32_t volume);
    int32_t getVolume();
    void setNotify(bool notify);
    void setNotifyCallback(HWND hwnd);

    // playback
    void play(int32_t from, int32_t to);
    void pause();
    void resume();
    void stop();
    void seekBegin();
    void seekEnd();
    void seekTo(int32_t to);

    // worms 2 plus extension
    void loadVolume();
    void MonitorDirectoryThread(struct ThreadData* data);
    void MonitorDirectory(wchar_t* directoryPath, wchar_t* targetFileName);

private:
    ZPlay* m_player;
    CDTrackList m_tracks;
    int32_t m_currentTrack;
    int32_t m_timeFormat;
    int32_t m_notifyMessage;
    HWND m_notifyHwnd;

    static int32_t WINAPI callback(void* instance, void* user_data,
        TCallbackMessage message, unsigned int param1, unsigned int param2);

    void notify();
    void getCurrentPosition(CDTime& position);
};