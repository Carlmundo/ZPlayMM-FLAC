#pragma once

#include "CDPlayer.hpp"

#include <cstdint>
#include <windows.h>

typedef MCIERROR(WINAPI* mciSendCommandA_t)(
    MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR);
typedef MCIERROR(WINAPI* mciSendStringA_t)(LPCSTR, LPCSTR, UINT, HWND);

class WinMM
{
public:
    WinMM();
    void load(HINSTANCE hinstDLL);
    MCIERROR mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg,
        DWORD_PTR fdwCommand, DWORD_PTR dwParam);
    MCIERROR mciSendStringA(
        LPCTSTR cmd, LPTSTR ret, UINT cchReturn, HWND hwndCallback);
    UINT auxGetNumDevs();
    MMRESULT auxGetDevCapsA(UINT_PTR uDeviceID, LPAUXCAPS lpCaps, UINT cbCaps);
    MMRESULT auxGetVolume(UINT uDeviceID, LPDWORD lpdwVolume);
    MMRESULT auxSetVolume(UINT uDeviceID, DWORD dwVolume);

private:
    mciSendCommandA_t m_mciSendCommandA;
    mciSendStringA_t m_mciSendStringA;
    CDPlayer* m_player;
    DWORD m_timeFormat;
    DWORD m_volume;

    MCIERROR commandResume(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam);
    MCIERROR commandOpen(
        MCIDEVICEID IDDevice, DWORD_PTR fdwCommand, LPMCI_OPEN_PARMS dwParam);
    MCIERROR commandClose(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam);
    MCIERROR commandSet(DWORD_PTR fdwCommand, LPMCI_SET_PARMS dwParam);
    MCIERROR commandSeek(DWORD_PTR fdwCommand, LPMCI_SEEK_PARMS dwParam);
    MCIERROR commandPlay(DWORD_PTR fdwCommand, LPMCI_PLAY_PARMS dwParam);
    MCIERROR commandStop(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam);
    MCIERROR commandPause(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam);
    MCIERROR commandStatus(DWORD_PTR fdwCommand, LPMCI_STATUS_PARMS dwParam);
};