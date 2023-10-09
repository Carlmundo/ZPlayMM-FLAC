#include "ZPlayMM.hpp"
#include "Logger.hpp"
#include "WinMM.hpp"
#include "WinMMError.hpp"

#include <mutex>

static const LPCSTR exportNames[] = {
    "CloseDriver", "DefDriverProc", "DriverCallback", "DrvGetModuleHandle",
    "GetDriverModuleHandle", "OpenDriver", "PlaySound", "PlaySoundA",
    "PlaySoundW", "SendDriverMessage", "WOWAppExit", "auxGetDevCapsA",
    "auxGetDevCapsW", "auxGetNumDevs", "auxGetVolume", "auxOutMessage",
    "auxSetVolume", "joyConfigChanged", "joyGetDevCapsA", "joyGetDevCapsW",
    "joyGetNumDevs", "joyGetPos", "joyGetPosEx", "joyGetThreshold",
    "joyReleaseCapture", "joySetCapture", "joySetThreshold", "mciDriverNotify",
    "mciDriverYield", "mciExecute", "mciFreeCommandResource",
    "mciGetCreatorTask", "mciGetDeviceIDA", "mciGetDeviceIDFromElementIDA",
    "mciGetDeviceIDFromElementIDW", "mciGetDeviceIDW", "mciGetDriverData",
    "mciGetErrorStringA", "mciGetErrorStringW", "mciGetYieldProc",
    "mciLoadCommandResource", "mciSendCommandA", "mciSendCommandW",
    "mciSendStringA", "mciSendStringW", "mciSetDriverData", "mciSetYieldProc",
    "midiConnect", "midiDisconnect", "midiInAddBuffer", "midiInClose",
    "midiInGetDevCapsA", "midiInGetDevCapsW", "midiInGetErrorTextA",
    "midiInGetErrorTextW", "midiInGetID", "midiInGetNumDevs", "midiInMessage",
    "midiInOpen", "midiInPrepareHeader", "midiInReset", "midiInStart",
    "midiInStop", "midiInUnprepareHeader", "midiOutCacheDrumPatches",
    "midiOutCachePatches", "midiOutClose", "midiOutGetDevCapsA",
    "midiOutGetDevCapsW", "midiOutGetErrorTextA", "midiOutGetErrorTextW",
    "midiOutGetID", "midiOutGetNumDevs", "midiOutGetVolume", "midiOutLongMsg",
    "midiOutMessage", "midiOutOpen", "midiOutPrepareHeader", "midiOutReset",
    "midiOutSetVolume", "midiOutShortMsg", "midiOutUnprepareHeader",
    "midiStreamClose", "midiStreamOpen", "midiStreamOut", "midiStreamPause",
    "midiStreamPosition", "midiStreamProperty", "midiStreamRestart",
    "midiStreamStop", "mixerClose", "mixerGetControlDetailsA",
    "mixerGetControlDetailsW", "mixerGetDevCapsA", "mixerGetDevCapsW",
    "mixerGetID", "mixerGetLineControlsA", "mixerGetLineControlsW",
    "mixerGetLineInfoA", "mixerGetLineInfoW", "mixerGetNumDevs", "mixerMessage",
    "mixerOpen", "mixerSetControlDetails", "mmDrvInstall", "mmGetCurrentTask",
    "mmTaskBlock", "mmTaskCreate", "mmTaskSignal", "mmTaskYield", "mmioAdvance",
    "mmioAscend", "mmioClose", "mmioCreateChunk", "mmioDescend", "mmioFlush",
    "mmioGetInfo", "mmioInstallIOProcA", "mmioInstallIOProcW", "mmioOpenA",
    "mmioOpenW", "mmioRead", "mmioRenameA", "mmioRenameW", "mmioSeek",
    "mmioSendMessage", "mmioSetBuffer", "mmioSetInfo", "mmioStringToFOURCCA",
    "mmioStringToFOURCCW", "mmioWrite", "mmsystemGetVersion", "sndPlaySoundA",
    "sndPlaySoundW", "timeBeginPeriod", "timeEndPeriod", "timeGetDevCaps",
    "timeGetSystemTime", "timeGetTime", "timeKillEvent", "timeSetEvent",
    "waveInAddBuffer", "waveInClose", "waveInGetDevCapsA", "waveInGetDevCapsW",
    "waveInGetErrorTextA", "waveInGetErrorTextW", "waveInGetID",
    "waveInGetNumDevs", "waveInGetPosition", "waveInMessage", "waveInOpen",
    "waveInPrepareHeader", "waveInReset", "waveInStart", "waveInStop",
    "waveInUnprepareHeader", "waveOutBreakLoop", "waveOutClose",
    "waveOutGetDevCapsA", "waveOutGetDevCapsW", "waveOutGetErrorTextA",
    "waveOutGetErrorTextW", "waveOutGetID", "waveOutGetNumDevs",
    "waveOutGetPitch", "waveOutGetPlaybackRate", "waveOutGetPosition",
    "waveOutGetVolume", "waveOutMessage", "waveOutOpen", "waveOutPause",
    "waveOutPrepareHeader", "waveOutReset", "waveOutRestart", "waveOutSetPitch",
    "waveOutSetPlaybackRate", "waveOutSetVolume", "waveOutUnprepareHeader",
    "waveOutWrite",
};

static const size_t exportCount = sizeof(exportNames) / sizeof(exportNames[0]);
static FARPROC exportProcs[exportCount];

static WinMM winmm;
static std::mutex mutex;
static HINSTANCE hinstDLL = nullptr;

MCIERROR HandleException()
{
    try {
        throw;
    } catch (const WinMMError& ex) {
        LOG_INFO("%s (0x%x %s)", ex.what(), ex.getErrorCode(),
            ex.getErrorName().c_str());
        return ex.getErrorCode();
    } catch (const std::runtime_error& ex) {
        MessageBox(nullptr, ex.what(), nullptr, MB_OK | MB_ICONERROR);
        return MMSYSERR_ERROR;
    } catch (const std::logic_error& ex) {
        MessageBox(nullptr, ex.what(), nullptr, MB_OK | MB_ICONERROR);
        return MMSYSERR_ERROR;
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    try {
        switch (fdwReason) {
            case DLL_PROCESS_ATTACH: {
                char sysDir[MAX_PATH];
                if (!GetSystemDirectory(sysDir, sizeof(sysDir))) {
                    throw std::runtime_error("Can't get system directory!");
                }

                char dllPath[MAX_PATH];
                sprintf_s(dllPath, sizeof(dllPath), "%s\\winmm.dll", sysDir);

                hinstDLL = LoadLibrary(dllPath);

                for (size_t i = 0; i < exportCount; i++) {
                    exportProcs[i] = GetProcAddress(hinstDLL, exportNames[i]);

                    if (!exportProcs[i]) {
                        // don't throw exceptions here, the address may be
                        // unused anyway
                        LOG_INFO("Failed to get proc address for %s!",
                            exportNames[i]);
                    }

                    // crappy code generator
                    //LOG_INFO("DECLARE_DLL_JMP(jmp_% -32s, int32_t, exportProcs "
                    //         "+ % 4d)",
                    //    exportNames[i], i * 4);
                }

                winmm.load(hinstDLL);

                break;
            }

            case DLL_PROCESS_DETACH: {
                if (hinstDLL) {
                    FreeLibrary(hinstDLL);
                }
                break;
            }
        }
    } catch (...) {
        HandleException();
    }

    return TRUE;
}

// list of wrapped functions
MCIERROR WINAPI wrap_mciSendCommandA(
    MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    std::lock_guard<std::mutex> lock(mutex);

    try {
        return winmm.mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
    } catch (...) {
        return HandleException();
    }
}

MCIERROR WINAPI wrap_mciSendStringA(
    LPCTSTR cmd, LPTSTR ret, UINT cchReturn, HWND hwndCallback)
{
    std::lock_guard<std::mutex> lock(mutex);

    try {
        return winmm.mciSendStringA(cmd, ret, cchReturn, hwndCallback);
    } catch (...) {
        return HandleException();
    }
}

UINT WINAPI wrap_auxGetNumDevs()
{
    std::lock_guard<std::mutex> lock(mutex);

    try {
        return winmm.auxGetNumDevs();
    } catch (...) {
        return HandleException();
    }
}

MMRESULT WINAPI wrap_auxGetDevCapsA(
    UINT_PTR uDeviceID, LPAUXCAPS lpCaps, UINT cbCaps)
{
    std::lock_guard<std::mutex> lock(mutex);

    try {
        return winmm.auxGetDevCapsA(uDeviceID, lpCaps, cbCaps);
    } catch (...) {
        return HandleException();
    }
}

MMRESULT WINAPI wrap_auxGetVolume(UINT uDeviceID, LPDWORD lpdwVolume)
{
    std::lock_guard<std::mutex> lock(mutex);

    try {
        return winmm.auxGetVolume(uDeviceID, lpdwVolume);
    } catch (...) {
        return HandleException();
    }
}

MMRESULT WINAPI wrap_auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
    std::lock_guard<std::mutex> lock(mutex);

    try {
        return winmm.auxSetVolume(uDeviceID, dwVolume);
    } catch (...) {
        return HandleException();
    }
}

// list of DLL jump exports
DECLARE_DLL_JMP(jmp_CloseDriver, int32_t, exportProcs + 0)
DECLARE_DLL_JMP(jmp_DefDriverProc, int32_t, exportProcs + 4)
DECLARE_DLL_JMP(jmp_DriverCallback, int32_t, exportProcs + 8)
DECLARE_DLL_JMP(jmp_DrvGetModuleHandle, int32_t, exportProcs + 12)
DECLARE_DLL_JMP(jmp_GetDriverModuleHandle, int32_t, exportProcs + 16)
DECLARE_DLL_JMP(jmp_OpenDriver, int32_t, exportProcs + 20)
DECLARE_DLL_JMP(jmp_PlaySound, int32_t, exportProcs + 24)
DECLARE_DLL_JMP(jmp_PlaySoundA, int32_t, exportProcs + 28)
DECLARE_DLL_JMP(jmp_PlaySoundW, int32_t, exportProcs + 32)
DECLARE_DLL_JMP(jmp_SendDriverMessage, int32_t, exportProcs + 36)
DECLARE_DLL_JMP(jmp_WOWAppExit, int32_t, exportProcs + 40)
DECLARE_DLL_JMP(jmp_auxGetDevCapsA, int32_t, exportProcs + 44)
DECLARE_DLL_JMP(jmp_auxGetDevCapsW, int32_t, exportProcs + 48)
DECLARE_DLL_JMP(jmp_auxGetNumDevs, int32_t, exportProcs + 52)
DECLARE_DLL_JMP(jmp_auxGetVolume, int32_t, exportProcs + 56)
DECLARE_DLL_JMP(jmp_auxOutMessage, int32_t, exportProcs + 60)
DECLARE_DLL_JMP(jmp_auxSetVolume, int32_t, exportProcs + 64)
DECLARE_DLL_JMP(jmp_joyConfigChanged, int32_t, exportProcs + 68)
DECLARE_DLL_JMP(jmp_joyGetDevCapsA, int32_t, exportProcs + 72)
DECLARE_DLL_JMP(jmp_joyGetDevCapsW, int32_t, exportProcs + 76)
DECLARE_DLL_JMP(jmp_joyGetNumDevs, int32_t, exportProcs + 80)
DECLARE_DLL_JMP(jmp_joyGetPos, int32_t, exportProcs + 84)
DECLARE_DLL_JMP(jmp_joyGetPosEx, int32_t, exportProcs + 88)
DECLARE_DLL_JMP(jmp_joyGetThreshold, int32_t, exportProcs + 92)
DECLARE_DLL_JMP(jmp_joyReleaseCapture, int32_t, exportProcs + 96)
DECLARE_DLL_JMP(jmp_joySetCapture, int32_t, exportProcs + 100)
DECLARE_DLL_JMP(jmp_joySetThreshold, int32_t, exportProcs + 104)
DECLARE_DLL_JMP(jmp_mciDriverNotify, int32_t, exportProcs + 108)
DECLARE_DLL_JMP(jmp_mciDriverYield, int32_t, exportProcs + 112)
DECLARE_DLL_JMP(jmp_mciExecute, int32_t, exportProcs + 116)
DECLARE_DLL_JMP(jmp_mciFreeCommandResource, int32_t, exportProcs + 120)
DECLARE_DLL_JMP(jmp_mciGetCreatorTask, int32_t, exportProcs + 124)
DECLARE_DLL_JMP(jmp_mciGetDeviceIDA, int32_t, exportProcs + 128)
DECLARE_DLL_JMP(jmp_mciGetDeviceIDFromElementIDA, int32_t, exportProcs + 132)
DECLARE_DLL_JMP(jmp_mciGetDeviceIDFromElementIDW, int32_t, exportProcs + 136)
DECLARE_DLL_JMP(jmp_mciGetDeviceIDW, int32_t, exportProcs + 140)
DECLARE_DLL_JMP(jmp_mciGetDriverData, int32_t, exportProcs + 144)
DECLARE_DLL_JMP(jmp_mciGetErrorStringA, int32_t, exportProcs + 148)
DECLARE_DLL_JMP(jmp_mciGetErrorStringW, int32_t, exportProcs + 152)
DECLARE_DLL_JMP(jmp_mciGetYieldProc, int32_t, exportProcs + 156)
DECLARE_DLL_JMP(jmp_mciLoadCommandResource, int32_t, exportProcs + 160)
DECLARE_DLL_JMP(jmp_mciSendCommandA, int32_t, exportProcs + 164)
DECLARE_DLL_JMP(jmp_mciSendCommandW, int32_t, exportProcs + 168)
DECLARE_DLL_JMP(jmp_mciSendStringA, int32_t, exportProcs + 172)
DECLARE_DLL_JMP(jmp_mciSendStringW, int32_t, exportProcs + 176)
DECLARE_DLL_JMP(jmp_mciSetDriverData, int32_t, exportProcs + 180)
DECLARE_DLL_JMP(jmp_mciSetYieldProc, int32_t, exportProcs + 184)
DECLARE_DLL_JMP(jmp_midiConnect, int32_t, exportProcs + 188)
DECLARE_DLL_JMP(jmp_midiDisconnect, int32_t, exportProcs + 192)
DECLARE_DLL_JMP(jmp_midiInAddBuffer, int32_t, exportProcs + 196)
DECLARE_DLL_JMP(jmp_midiInClose, int32_t, exportProcs + 200)
DECLARE_DLL_JMP(jmp_midiInGetDevCapsA, int32_t, exportProcs + 204)
DECLARE_DLL_JMP(jmp_midiInGetDevCapsW, int32_t, exportProcs + 208)
DECLARE_DLL_JMP(jmp_midiInGetErrorTextA, int32_t, exportProcs + 212)
DECLARE_DLL_JMP(jmp_midiInGetErrorTextW, int32_t, exportProcs + 216)
DECLARE_DLL_JMP(jmp_midiInGetID, int32_t, exportProcs + 220)
DECLARE_DLL_JMP(jmp_midiInGetNumDevs, int32_t, exportProcs + 224)
DECLARE_DLL_JMP(jmp_midiInMessage, int32_t, exportProcs + 228)
DECLARE_DLL_JMP(jmp_midiInOpen, int32_t, exportProcs + 232)
DECLARE_DLL_JMP(jmp_midiInPrepareHeader, int32_t, exportProcs + 236)
DECLARE_DLL_JMP(jmp_midiInReset, int32_t, exportProcs + 240)
DECLARE_DLL_JMP(jmp_midiInStart, int32_t, exportProcs + 244)
DECLARE_DLL_JMP(jmp_midiInStop, int32_t, exportProcs + 248)
DECLARE_DLL_JMP(jmp_midiInUnprepareHeader, int32_t, exportProcs + 252)
DECLARE_DLL_JMP(jmp_midiOutCacheDrumPatches, int32_t, exportProcs + 256)
DECLARE_DLL_JMP(jmp_midiOutCachePatches, int32_t, exportProcs + 260)
DECLARE_DLL_JMP(jmp_midiOutClose, int32_t, exportProcs + 264)
DECLARE_DLL_JMP(jmp_midiOutGetDevCapsA, int32_t, exportProcs + 268)
DECLARE_DLL_JMP(jmp_midiOutGetDevCapsW, int32_t, exportProcs + 272)
DECLARE_DLL_JMP(jmp_midiOutGetErrorTextA, int32_t, exportProcs + 276)
DECLARE_DLL_JMP(jmp_midiOutGetErrorTextW, int32_t, exportProcs + 280)
DECLARE_DLL_JMP(jmp_midiOutGetID, int32_t, exportProcs + 284)
DECLARE_DLL_JMP(jmp_midiOutGetNumDevs, int32_t, exportProcs + 288)
DECLARE_DLL_JMP(jmp_midiOutGetVolume, int32_t, exportProcs + 292)
DECLARE_DLL_JMP(jmp_midiOutLongMsg, int32_t, exportProcs + 296)
DECLARE_DLL_JMP(jmp_midiOutMessage, int32_t, exportProcs + 300)
DECLARE_DLL_JMP(jmp_midiOutOpen, int32_t, exportProcs + 304)
DECLARE_DLL_JMP(jmp_midiOutPrepareHeader, int32_t, exportProcs + 308)
DECLARE_DLL_JMP(jmp_midiOutReset, int32_t, exportProcs + 312)
DECLARE_DLL_JMP(jmp_midiOutSetVolume, int32_t, exportProcs + 316)
DECLARE_DLL_JMP(jmp_midiOutShortMsg, int32_t, exportProcs + 320)
DECLARE_DLL_JMP(jmp_midiOutUnprepareHeader, int32_t, exportProcs + 324)
DECLARE_DLL_JMP(jmp_midiStreamClose, int32_t, exportProcs + 328)
DECLARE_DLL_JMP(jmp_midiStreamOpen, int32_t, exportProcs + 332)
DECLARE_DLL_JMP(jmp_midiStreamOut, int32_t, exportProcs + 336)
DECLARE_DLL_JMP(jmp_midiStreamPause, int32_t, exportProcs + 340)
DECLARE_DLL_JMP(jmp_midiStreamPosition, int32_t, exportProcs + 344)
DECLARE_DLL_JMP(jmp_midiStreamProperty, int32_t, exportProcs + 348)
DECLARE_DLL_JMP(jmp_midiStreamRestart, int32_t, exportProcs + 352)
DECLARE_DLL_JMP(jmp_midiStreamStop, int32_t, exportProcs + 356)
DECLARE_DLL_JMP(jmp_mixerClose, int32_t, exportProcs + 360)
DECLARE_DLL_JMP(jmp_mixerGetControlDetailsA, int32_t, exportProcs + 364)
DECLARE_DLL_JMP(jmp_mixerGetControlDetailsW, int32_t, exportProcs + 368)
DECLARE_DLL_JMP(jmp_mixerGetDevCapsA, int32_t, exportProcs + 372)
DECLARE_DLL_JMP(jmp_mixerGetDevCapsW, int32_t, exportProcs + 376)
DECLARE_DLL_JMP(jmp_mixerGetID, int32_t, exportProcs + 380)
DECLARE_DLL_JMP(jmp_mixerGetLineControlsA, int32_t, exportProcs + 384)
DECLARE_DLL_JMP(jmp_mixerGetLineControlsW, int32_t, exportProcs + 388)
DECLARE_DLL_JMP(jmp_mixerGetLineInfoA, int32_t, exportProcs + 392)
DECLARE_DLL_JMP(jmp_mixerGetLineInfoW, int32_t, exportProcs + 396)
DECLARE_DLL_JMP(jmp_mixerGetNumDevs, int32_t, exportProcs + 400)
DECLARE_DLL_JMP(jmp_mixerMessage, int32_t, exportProcs + 404)
DECLARE_DLL_JMP(jmp_mixerOpen, int32_t, exportProcs + 408)
DECLARE_DLL_JMP(jmp_mixerSetControlDetails, int32_t, exportProcs + 412)
DECLARE_DLL_JMP(jmp_mmDrvInstall, int32_t, exportProcs + 416)
DECLARE_DLL_JMP(jmp_mmGetCurrentTask, int32_t, exportProcs + 420)
DECLARE_DLL_JMP(jmp_mmTaskBlock, int32_t, exportProcs + 424)
DECLARE_DLL_JMP(jmp_mmTaskCreate, int32_t, exportProcs + 428)
DECLARE_DLL_JMP(jmp_mmTaskSignal, int32_t, exportProcs + 432)
DECLARE_DLL_JMP(jmp_mmTaskYield, int32_t, exportProcs + 436)
DECLARE_DLL_JMP(jmp_mmioAdvance, int32_t, exportProcs + 440)
DECLARE_DLL_JMP(jmp_mmioAscend, int32_t, exportProcs + 444)
DECLARE_DLL_JMP(jmp_mmioClose, int32_t, exportProcs + 448)
DECLARE_DLL_JMP(jmp_mmioCreateChunk, int32_t, exportProcs + 452)
DECLARE_DLL_JMP(jmp_mmioDescend, int32_t, exportProcs + 456)
DECLARE_DLL_JMP(jmp_mmioFlush, int32_t, exportProcs + 460)
DECLARE_DLL_JMP(jmp_mmioGetInfo, int32_t, exportProcs + 464)
DECLARE_DLL_JMP(jmp_mmioInstallIOProcA, int32_t, exportProcs + 468)
DECLARE_DLL_JMP(jmp_mmioInstallIOProcW, int32_t, exportProcs + 472)
DECLARE_DLL_JMP(jmp_mmioOpenA, int32_t, exportProcs + 476)
DECLARE_DLL_JMP(jmp_mmioOpenW, int32_t, exportProcs + 480)
DECLARE_DLL_JMP(jmp_mmioRead, int32_t, exportProcs + 484)
DECLARE_DLL_JMP(jmp_mmioRenameA, int32_t, exportProcs + 488)
DECLARE_DLL_JMP(jmp_mmioRenameW, int32_t, exportProcs + 492)
DECLARE_DLL_JMP(jmp_mmioSeek, int32_t, exportProcs + 496)
DECLARE_DLL_JMP(jmp_mmioSendMessage, int32_t, exportProcs + 500)
DECLARE_DLL_JMP(jmp_mmioSetBuffer, int32_t, exportProcs + 504)
DECLARE_DLL_JMP(jmp_mmioSetInfo, int32_t, exportProcs + 508)
DECLARE_DLL_JMP(jmp_mmioStringToFOURCCA, int32_t, exportProcs + 512)
DECLARE_DLL_JMP(jmp_mmioStringToFOURCCW, int32_t, exportProcs + 516)
DECLARE_DLL_JMP(jmp_mmioWrite, int32_t, exportProcs + 520)
DECLARE_DLL_JMP(jmp_mmsystemGetVersion, int32_t, exportProcs + 524)
DECLARE_DLL_JMP(jmp_sndPlaySoundA, int32_t, exportProcs + 528)
DECLARE_DLL_JMP(jmp_sndPlaySoundW, int32_t, exportProcs + 532)
DECLARE_DLL_JMP(jmp_timeBeginPeriod, int32_t, exportProcs + 536)
DECLARE_DLL_JMP(jmp_timeEndPeriod, int32_t, exportProcs + 540)
DECLARE_DLL_JMP(jmp_timeGetDevCaps, int32_t, exportProcs + 544)
DECLARE_DLL_JMP(jmp_timeGetSystemTime, int32_t, exportProcs + 548)
DECLARE_DLL_JMP(jmp_timeGetTime, int32_t, exportProcs + 552)
DECLARE_DLL_JMP(jmp_timeKillEvent, int32_t, exportProcs + 556)
DECLARE_DLL_JMP(jmp_timeSetEvent, int32_t, exportProcs + 560)
DECLARE_DLL_JMP(jmp_waveInAddBuffer, int32_t, exportProcs + 564)
DECLARE_DLL_JMP(jmp_waveInClose, int32_t, exportProcs + 568)
DECLARE_DLL_JMP(jmp_waveInGetDevCapsA, int32_t, exportProcs + 572)
DECLARE_DLL_JMP(jmp_waveInGetDevCapsW, int32_t, exportProcs + 576)
DECLARE_DLL_JMP(jmp_waveInGetErrorTextA, int32_t, exportProcs + 580)
DECLARE_DLL_JMP(jmp_waveInGetErrorTextW, int32_t, exportProcs + 584)
DECLARE_DLL_JMP(jmp_waveInGetID, int32_t, exportProcs + 588)
DECLARE_DLL_JMP(jmp_waveInGetNumDevs, int32_t, exportProcs + 592)
DECLARE_DLL_JMP(jmp_waveInGetPosition, int32_t, exportProcs + 596)
DECLARE_DLL_JMP(jmp_waveInMessage, int32_t, exportProcs + 600)
DECLARE_DLL_JMP(jmp_waveInOpen, int32_t, exportProcs + 604)
DECLARE_DLL_JMP(jmp_waveInPrepareHeader, int32_t, exportProcs + 608)
DECLARE_DLL_JMP(jmp_waveInReset, int32_t, exportProcs + 612)
DECLARE_DLL_JMP(jmp_waveInStart, int32_t, exportProcs + 616)
DECLARE_DLL_JMP(jmp_waveInStop, int32_t, exportProcs + 620)
DECLARE_DLL_JMP(jmp_waveInUnprepareHeader, int32_t, exportProcs + 624)
DECLARE_DLL_JMP(jmp_waveOutBreakLoop, int32_t, exportProcs + 628)
DECLARE_DLL_JMP(jmp_waveOutClose, int32_t, exportProcs + 632)
DECLARE_DLL_JMP(jmp_waveOutGetDevCapsA, int32_t, exportProcs + 636)
DECLARE_DLL_JMP(jmp_waveOutGetDevCapsW, int32_t, exportProcs + 640)
DECLARE_DLL_JMP(jmp_waveOutGetErrorTextA, int32_t, exportProcs + 644)
DECLARE_DLL_JMP(jmp_waveOutGetErrorTextW, int32_t, exportProcs + 648)
DECLARE_DLL_JMP(jmp_waveOutGetID, int32_t, exportProcs + 652)
DECLARE_DLL_JMP(jmp_waveOutGetNumDevs, int32_t, exportProcs + 656)
DECLARE_DLL_JMP(jmp_waveOutGetPitch, int32_t, exportProcs + 660)
DECLARE_DLL_JMP(jmp_waveOutGetPlaybackRate, int32_t, exportProcs + 664)
DECLARE_DLL_JMP(jmp_waveOutGetPosition, int32_t, exportProcs + 668)
DECLARE_DLL_JMP(jmp_waveOutGetVolume, int32_t, exportProcs + 672)
DECLARE_DLL_JMP(jmp_waveOutMessage, int32_t, exportProcs + 676)
DECLARE_DLL_JMP(jmp_waveOutOpen, int32_t, exportProcs + 680)
DECLARE_DLL_JMP(jmp_waveOutPause, int32_t, exportProcs + 684)
DECLARE_DLL_JMP(jmp_waveOutPrepareHeader, int32_t, exportProcs + 688)
DECLARE_DLL_JMP(jmp_waveOutReset, int32_t, exportProcs + 692)
DECLARE_DLL_JMP(jmp_waveOutRestart, int32_t, exportProcs + 696)
DECLARE_DLL_JMP(jmp_waveOutSetPitch, int32_t, exportProcs + 700)
DECLARE_DLL_JMP(jmp_waveOutSetPlaybackRate, int32_t, exportProcs + 704)
DECLARE_DLL_JMP(jmp_waveOutSetVolume, int32_t, exportProcs + 708)
DECLARE_DLL_JMP(jmp_waveOutUnprepareHeader, int32_t, exportProcs + 712)
DECLARE_DLL_JMP(jmp_waveOutWrite, int32_t, exportProcs + 716)
