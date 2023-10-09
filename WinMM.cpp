#include "WinMM.hpp"
#include "Logger.hpp"

#include <stdio.h>

#define MAGIC_DEVICEID 0xCDFACADE
#define MUSIC_DIR "music"

WinMM::WinMM()
    : m_mciSendCommandA(nullptr)
    , m_mciSendStringA(nullptr)
{
}

void WinMM::load(HINSTANCE hinstDLL)
{
    m_mciSendCommandA = reinterpret_cast<mciSendCommandA_t>(
        GetProcAddress(hinstDLL, "mciSendCommandA"));
    m_mciSendStringA = reinterpret_cast<mciSendStringA_t>(
        GetProcAddress(hinstDLL, "mciSendStringA"));
}

MCIERROR WinMM::commandResume(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam)
{
    LOG_TRACE("  MCI_RESUME");

    m_player->resume();

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::commandOpen(
    MCIDEVICEID IDDevice, DWORD_PTR fdwCommand, LPMCI_OPEN_PARMS dwParam)
{
    LOG_TRACE("  MCI_OPEN");

    if (fdwCommand & MCI_OPEN_ALIAS) {
        LOG_TRACE("    MCI_OPEN_ALIAS");
        LOG_TRACE("      lpstrAlias = %s", dwParam->lpstrAlias);
    }

    if (fdwCommand & MCI_OPEN_ELEMENT) {
        LOG_TRACE("    MCI_OPEN_ELEMENT");

        if (fdwCommand & MCI_OPEN_ELEMENT_ID) {
            LOG_TRACE("      MCI_OPEN_ELEMENT_ID");
            LOG_TRACE("      lpstrElementName = %d", dwParam->lpstrElementName);
        } else {
            LOG_TRACE("      lpstrElementName = %s", dwParam->lpstrElementName);
        }
    }

    if (fdwCommand & MCI_OPEN_SHAREABLE) {
        LOG_TRACE("    MCI_OPEN_SHAREABLE");
    }

    if (fdwCommand & MCI_OPEN_TYPE) {
        LOG_TRACE("    MCI_OPEN_TYPE");

        if (fdwCommand & MCI_OPEN_TYPE_ID) {
            LOG_TRACE("      MCI_OPEN_TYPE_ID");

            if (LOWORD(dwParam->lpstrDeviceType) != MCI_DEVTYPE_CD_AUDIO) {
                goto skip;
            }
        } else {
            LOG_TRACE("      dwParam = %s", dwParam->lpstrDeviceType);

            if (strcmp(dwParam->lpstrDeviceType, "cdaudio") != 0) {
                goto skip;
            }
        }

        if (m_player) {
            return MCIERR_DEVICE_OPEN;
        }

        m_player = new CDPlayer();
        dwParam->wDeviceID = m_player->getDeviceID();

        return MMSYSERR_NOERROR;
    }

skip:
    // command is not for cdaudio
    return m_mciSendCommandA(
        IDDevice, MCI_OPEN, fdwCommand, reinterpret_cast<DWORD_PTR>(dwParam));
}

MCIERROR WinMM::commandClose(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam)
{
    LOG_TRACE("  MCI_CLOSE");

    if (m_player) {
        delete m_player;
        m_player = nullptr;
    }

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::commandSet(DWORD_PTR fdwCommand, LPMCI_SET_PARMS dwParam)
{
    LOG_TRACE("  MCI_SET");

    if (fdwCommand & MCI_SET_TIME_FORMAT) {
        LOG_TRACE("    MCI_SET_TIME_FORMAT");

        // Only MCI_FORMAT_MILLISECONDS, MCI_FORMAT_MSF and MCI_FORMAT_TMSF
        // are allowed for cdaudio devices
        switch (dwParam->dwTimeFormat) {
            case MCI_FORMAT_MILLISECONDS:
                LOG_TRACE("      MCI_FORMAT_MILLISECONDS");
                break;
            case MCI_FORMAT_MSF:
                LOG_TRACE("      MCI_FORMAT_MSF");
                break;
            case MCI_FORMAT_TMSF:
                LOG_TRACE("      MCI_FORMAT_TMSF");
                break;
            default:
                return MMSYSERR_INVALPARAM;
        }

        m_player->setTimeFormat(dwParam->dwTimeFormat);
    }

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::commandSeek(DWORD_PTR fdwCommand, LPMCI_SEEK_PARMS dwParam)
{
    LOG_TRACE("  MCI_SEEK");

    if (fdwCommand & MCI_SEEK_TO_START) {
        LOG_TRACE("    MCI_SEEK_TO_START");
        m_player->seekBegin();
    }

    if (fdwCommand & MCI_SEEK_TO_END) {
        LOG_TRACE("    MCI_SEEK_TO_END");
        m_player->seekEnd();
    }

    if (fdwCommand & MCI_TO) {
        LOG_TRACE("    MCI_TO");
        LOG_TRACE("      dwTo = %d", dwParam->dwTo);

        m_player->seekTo(dwParam->dwTo);
    }

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::commandPlay(DWORD_PTR fdwCommand, LPMCI_PLAY_PARMS dwParam)
{
    LOG_TRACE("  MCI_PLAY");

    int32_t from = 0;
    if (fdwCommand & MCI_FROM) {
        LOG_TRACE("    MCI_FROM");
        from = dwParam->dwFrom;
    }

    int32_t to = 0;
    if (fdwCommand & MCI_TO) {
        LOG_TRACE("    MCI_TO");
        to = dwParam->dwTo;
    }

    m_player->play(from, to);

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::commandStop(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam)
{
    LOG_TRACE("  MCI_STOP");

    m_player->stop();

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::commandPause(DWORD_PTR fdwCommand, LPMCI_GENERIC_PARMS dwParam)
{
    LOG_TRACE("  MCI_PAUSE");

    m_player->pause();

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::commandStatus(DWORD_PTR fdwCommand, LPMCI_STATUS_PARMS dwParam)
{
    LOG_TRACE("  MCI_STATUS");

    dwParam->dwReturn = 0;

    if (fdwCommand & MCI_TRACK) {
        LOG_TRACE("    MCI_TRACK");
        LOG_TRACE("      dwTrack = %d", dwParam->dwTrack);
    }

    if (fdwCommand & MCI_STATUS_ITEM) {
        LOG_TRACE("    MCI_STATUS_ITEM");

        switch (dwParam->dwItem) {
            case MCI_STATUS_CURRENT_TRACK:
                LOG_TRACE("      MCI_STATUS_CURRENT_TRACK");
                dwParam->dwReturn = m_player->getCurrentTrack();
                break;

            case MCI_STATUS_LENGTH: {
                LOG_TRACE("      MCI_STATUS_LENGTH");
                if (fdwCommand & MCI_TRACK) {
                    dwParam->dwReturn = m_player->getLength(dwParam->dwTrack);
                } else {
                    dwParam->dwReturn = m_player->getLength(0);
                }
                break;
            }

            case MCI_STATUS_POSITION:
                LOG_TRACE("      MCI_STATUS_POSITION");
                if (fdwCommand & MCI_TRACK) {
                    dwParam->dwReturn = m_player->getPosition(dwParam->dwTrack);
                } else {
                    dwParam->dwReturn = m_player->getPosition(0);
                }
                break;

            case MCI_CDA_STATUS_TYPE_TRACK:
                LOG_TRACE("      MCI_CDA_STATUS_TYPE_TRACK");
                if (fdwCommand & MCI_TRACK) {
                    dwParam->dwReturn = m_player->getType(dwParam->dwTrack);
                } else {
                    // MCI_TRACK is required
                    return MCIERR_MISSING_PARAMETER;
                }
                break;

            case MCI_STATUS_MEDIA_PRESENT:
                LOG_TRACE("      MCI_STATUS_MEDIA_PRESENT");
                dwParam->dwReturn = m_player->getNumTracks() > 0;
                break;

            case MCI_STATUS_NUMBER_OF_TRACKS:
                LOG_TRACE("      MCI_STATUS_NUMBER_OF_TRACKS");
                dwParam->dwReturn = m_player->getNumTracks();
                break;

            case MCI_STATUS_MODE:
                LOG_TRACE("      MCI_STATUS_MODE");

                if (m_player->isPaused()) {
                    LOG_TRACE("        paused");
                    dwParam->dwReturn = MCI_MODE_PAUSE;
                } else if (m_player->isPlaying()) {
                    LOG_TRACE("        playing");
                    dwParam->dwReturn = MCI_MODE_PLAY;
                } else {
                    LOG_TRACE("        stopped");
                    dwParam->dwReturn = MCI_MODE_STOP;
                }
                break;

            case MCI_STATUS_READY:
                LOG_TRACE("      MCI_STATUS_READY");
                // always ready
                dwParam->dwReturn = TRUE;
                break;

            case MCI_STATUS_TIME_FORMAT:
                LOG_TRACE("      MCI_STATUS_TIME_FORMAT");
                dwParam->dwReturn = m_timeFormat;
                break;

            case MCI_STATUS_START:
                LOG_TRACE("      MCI_STATUS_START");
                dwParam->dwReturn = 0;
                break;
        }
    }

    LOG_TRACE("  dwReturn %d", dwParam->dwReturn);

    return MMSYSERR_NOERROR;
}

MCIERROR WinMM::mciSendCommandA(
    MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    LOG_TRACE("WinMM::mciSendCommandA(IDDevice=%p, uMsg=%p, fdwCommand=%p, "
           "dwParam=%p)",
        IDDevice, uMsg, fdwCommand, dwParam);

    if (uMsg != MCI_OPEN) {
        if (IDDevice != m_player->getDeviceID()) {
            // command is not for our device
            return m_mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
        }

        if (!m_player) {
            // device hasn't been opened yet
            return MCIERR_INVALID_DEVICE_NAME;
        }
    }

    if (fdwCommand & MCI_NOTIFY) {
        LOG_TRACE("  MCI_NOTIFY");
    }

    if (fdwCommand & MCI_WAIT) {
        LOG_TRACE("  MCI_WAIT");
    }

    MCIERROR result;

    switch (uMsg) {
        case MCI_OPEN:
            result = commandOpen(IDDevice, fdwCommand,
                reinterpret_cast<LPMCI_OPEN_PARMS>(dwParam));
            break;
        case MCI_CLOSE:
            result = commandClose(
                fdwCommand, reinterpret_cast<LPMCI_GENERIC_PARMS>(dwParam));
            break;
        case MCI_SET:
            result = commandSet(
                fdwCommand, reinterpret_cast<LPMCI_SET_PARMS>(dwParam));
            break;
        case MCI_SEEK:
            result = commandSeek(
                fdwCommand, reinterpret_cast<LPMCI_SEEK_PARMS>(dwParam));
            break;
        case MCI_PLAY:
            result = commandPlay(
                fdwCommand, reinterpret_cast<LPMCI_PLAY_PARMS>(dwParam));
            break;
        case MCI_STOP:
            result = commandStop(
                fdwCommand, reinterpret_cast<LPMCI_GENERIC_PARMS>(dwParam));
            break;
        case MCI_PAUSE:
            result = commandPause(
                fdwCommand, reinterpret_cast<LPMCI_GENERIC_PARMS>(dwParam));
            break;
        case MCI_RESUME:
            result = commandResume(
                fdwCommand, reinterpret_cast<LPMCI_GENERIC_PARMS>(dwParam));
            break;
        case MCI_STATUS:
            result = commandStatus(
                fdwCommand, reinterpret_cast<LPMCI_STATUS_PARMS>(dwParam));
            break;
        default:
            // unrecognized command
            return m_mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
    }

    if (m_player && result == MMSYSERR_NOERROR) {
        bool notify = (fdwCommand & MCI_NOTIFY) != 0;
        m_player->setNotify(notify);
        if (notify && dwParam) {
            LPMCI_GENERIC_PARMS parms =
                reinterpret_cast<LPMCI_GENERIC_PARMS>(dwParam);
            m_player->setNotifyCallback(
                reinterpret_cast<HWND>(parms->dwCallback));
        }
    }

    return result;
}

MCIERROR WinMM::mciSendStringA(
    LPCTSTR cmd, LPTSTR ret, UINT cchReturn, HWND hwndCallback)
{
    LOG_TRACE("cmd=%s", cmd);

    // this one is a big TODO
    return MMSYSERR_NOERROR;
}

UINT WinMM::auxGetNumDevs()
{
    LOG_TRACE("");

    // there can be only one
    return 1;
}

MMRESULT WinMM::auxGetDevCapsA(
    UINT_PTR uDeviceID, LPAUXCAPS lpCaps, UINT cbCaps)
{
    LOG_TRACE(
        "uDeviceID=%08X, lpCaps=%p, cbCaps=%08X", uDeviceID, lpCaps, cbCaps);

    lpCaps->wMid = 2;   // MM_CREATIVE
    lpCaps->wPid = 401; // MM_CREATIVE_AUX_CD
    lpCaps->vDriverVersion = 1;
    strcpy_s(lpCaps->szPname, sizeof(lpCaps->szPname), "ZPlayMM Virtual CD");
    lpCaps->wTechnology = AUXCAPS_CDAUDIO;
    lpCaps->dwSupport = AUXCAPS_VOLUME;

    return MMSYSERR_NOERROR;
}

MMRESULT WinMM::auxGetVolume(UINT uDeviceID, LPDWORD lpdwVolume)
{
    LOG_TRACE("uDeviceId=%08X, lpdwVolume=%p", uDeviceID, lpdwVolume);

    if (!m_player) {
        return MMSYSERR_BADDEVICEID;
    }

    *lpdwVolume = m_player->getVolume();

    return MMSYSERR_NOERROR;
}

MMRESULT WinMM::auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
    LOG_TRACE("WinMM::auxSetVolume(uDeviceId=%08X, dwVolume=%08X)", uDeviceID,
        dwVolume);

    if (!m_player) {
        return MMSYSERR_BADDEVICEID;
    }

    m_player->setVolume(dwVolume);

    return MMSYSERR_NOERROR;
}