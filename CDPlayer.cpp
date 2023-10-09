#include "CDPlayer.hpp"
#include "Logger.hpp"
#include "WinMMError.hpp"
#include <iostream>
#include <fstream>
#include <string.h>
using namespace std;

bool is_number(const std::string& s)
{
	return(strspn(s.c_str(), "0123456789") == s.size());
}

CDPlayer::CDPlayer()
    : m_player(CreateZPlay())
    , m_tracks("music", "Track", m_player)
    , m_currentTrack(1)
    , m_notifyMessage(0)
{
    m_player->SetCallbackFunc(&callback, MsgStop, this);
}

CDPlayer::~CDPlayer()
{
    m_player->Release();
    m_player = nullptr;
}

bool CDPlayer::isOpen()
{
    return m_player != nullptr;
}

bool CDPlayer::isPlaying()
{
    if (!m_player) {
        return false;
    }

    TStreamStatus streamStatus;
    m_player->GetStatus(&streamStatus);
    return streamStatus.fPlay != 0;
}

bool CDPlayer::isPaused()
{
    if (!m_player) {
        return false;
    }

    TStreamStatus streamStatus;
    m_player->GetStatus(&streamStatus);
    return streamStatus.fPause != 0;
}

int32_t CDPlayer::getNumTracks()
{
    return m_tracks.map().size();
}

int32_t CDPlayer::getCurrentTrack()
{
    return m_currentTrack;
}

int32_t CDPlayer::getLength(int32_t index)
{
    CDTime length = {};

    if (!index) {
        // return length of whole disc: position of last track plus its length
        const CDTrack& track = m_tracks.last();
        length.seconds = track.position.seconds + track.length.seconds;
    } else if (m_tracks.isValid(index)) {
        // return length of the selected track
        const CDTrack& track = m_tracks.get(index);
        length.seconds = track.length.seconds;
    } else {
        // invalid track
    }

    // The MSDN documentation doesn't mention it anywhere, but when
    // the current time format is set to TMSF, the length is actually
    // returned as MSF.
    int32_t timeFormat = m_timeFormat;
    if (timeFormat == MCI_FORMAT_TMSF) {
        timeFormat = MCI_FORMAT_MSF;
    }

    return length.toMciTime(timeFormat);
}

int32_t CDPlayer::getPosition(int32_t index)
{
    CDTime position = {};

    if (!index) {
        // position of the current track plus player position
        getCurrentPosition(position);
    } else if (m_tracks.isValid(index)) {
        if (m_timeFormat == MCI_FORMAT_TMSF) {
            // simply return the track index at second 0
            position.track = index;
            position.seconds = 0;
        } else {
            // position to the beginning of the selected track
            const CDTrack& track = m_tracks.get(index);
            position = track.position;
        }
    } else {
        // invalid track
    }

    return position.toMciTime(m_timeFormat);
}

void CDPlayer::getCurrentPosition(CDTime& position)
{
    // get current track index
    TStreamStatus streamStatus;
    m_player->GetStatus(&streamStatus);
    int32_t index = m_currentTrack + streamStatus.nSongIndex;

    if (m_timeFormat == MCI_FORMAT_TMSF) {
        // start at second 0
        position.track = index;
        position.seconds = 0;
    } else {
        // start at the absolute track position
        const CDTrack& track = m_tracks.get(index);
        position.track = index;
        position.seconds = track.position.seconds;
    }

    // add current stream time
    TStreamTime streamTime;
    m_player->GetPosition(&streamTime);
    position.seconds += streamTime.sec;
}

int32_t CDPlayer::getType(int32_t index)
{
    if (m_tracks.isAudio(index)) {
        return MCI_CDA_TRACK_AUDIO;
    } else {
        // threat data and missing tracks as "other"
        return MCI_CDA_TRACK_OTHER;
    }
}

int32_t CDPlayer::getDeviceID()
{
    return 0xCDFACADE;
}

void CDPlayer::setTimeFormat(int32_t timeFormat)
{
    m_timeFormat = timeFormat;
}

int32_t CDPlayer::getTimeFormat()
{
    return m_timeFormat;
}

void CDPlayer::setVolume(int32_t volume)
{
    uint32_t left = (LOWORD(volume) * 100) / 0xffff;
    uint32_t right = (HIWORD(volume) * 100) / 0xffff;

    m_player->SetPlayerVolume(left, right);
}

int32_t CDPlayer::getVolume()
{
    uint32_t left;
    uint32_t right;

    m_player->GetPlayerVolume(&left, &right);

    return MAKELONG(left * 0xffff / 100, right * 0xffff / 100);
}

void CDPlayer::setNotify(bool notify)
{
    m_notifyMessage = notify ? MCI_NOTIFY_SUCCESSFUL : 0;
}

void CDPlayer::setNotifyCallback(HWND hwnd)
{
    m_notifyHwnd = hwnd;
}

void CDPlayer::notify()
{
    if (m_notifyMessage && m_notifyHwnd) {
        LOG_TRACE("Posting MM_MCINOTIFY message %d to HWND %p", m_notifyMessage,
            m_notifyHwnd);
        PostMessage(m_notifyHwnd, MM_MCINOTIFY, m_notifyMessage, getDeviceID());

        // make sure the notification is send once only
        m_notifyMessage = 0;
        m_notifyHwnd = 0;
    }
}

int32_t WINAPI CDPlayer::callback(void* instance, void* user_data,
    TCallbackMessage message, uint32_t param1, uint32_t param2)
{
    LOG_TRACE("%d, %d, %d", message, param1, param2);

    CDPlayer* instancePlayer = static_cast<CDPlayer*>(user_data);
    instancePlayer->notify();

    return 0;
}

void CDPlayer::play(int32_t from, int32_t to)
{
    CDTime fromTime = {};
    if (from) {
        fromTime.fromMciTime(from, m_timeFormat);
        if (m_timeFormat != MCI_FORMAT_TMSF) {
            m_tracks.toTrackTime(fromTime);
        }
    } else {
        // "If MCI_FROM is not specified, the starting location defaults to the
        // current position."
        getCurrentPosition(fromTime);
    }

    CDTime toTime = {};
    if (to) {
        toTime.fromMciTime(to, m_timeFormat);
        if (m_timeFormat != MCI_FORMAT_TMSF) {
            m_tracks.toTrackTime(toTime);
        }
    } else {
        // "If MCI_TO is not specified, the ending location defaults to the end
        // of the media."
        const CDTrack& track = m_tracks.last();
        toTime.track = track.position.track;
        toTime.seconds = track.length.seconds;
    }

    if (!m_player->Close()) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }

    LOG_TRACE("Playing from %d:%d to %d:%d", fromTime.track,
        fromTime.seconds, toTime.track, toTime.seconds);

    // cancel if the track selection is invalid
    int32_t numTracks = m_tracks.map().size();
    if (fromTime.track > numTracks || fromTime.track < 1) {
        throw WinMMError(MCIERR_OUTOFRANGE);
    }

    if (toTime.track > numTracks || toTime.track < 1) {
        throw WinMMError(MCIERR_OUTOFRANGE);
    }

    if (toTime.track == fromTime.track &&
        toTime.seconds - fromTime.seconds < 1) {
        throw WinMMError(MCIERR_OUTOFRANGE);
    }

    if (toTime.track - fromTime.track < 0) {
        throw WinMMError(MCIERR_OUTOFRANGE);
    }

    for (int32_t i = fromTime.track; i <= toTime.track; i++) {
        // cancel if the playlist contains an unplayable track
        if (!m_tracks.isAudio(i)) {
            return;
        }

        // if the end position is the beginning of the next track, don't add it
        if (i == toTime.track && toTime.seconds == 0) {
            break;
        }

        const CDTrack& track = m_tracks.get(i);

		fstream file;
		string volumeContents;
		file.open("volumeBGM.txt", ios::in);
		if (file) {
			file >> volumeContents;
			if (is_number(volumeContents)){
				int volumeInt = stoi(volumeContents);
				if (volumeInt >= 0 && volumeInt <= 100) {
					m_player->SetPlayerVolume(volumeInt, volumeInt);
				}
			}	
		}
		
        // add file to queue
        if (!m_player->AddFile(track.path.c_str(), track.format)) {
            throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
        }
    }

    m_currentTrack = fromTime.track;

    // seek to start position
    if (fromTime.seconds > 0) {
        TStreamTime offset;
        offset.sec = fromTime.seconds;
        m_player->Seek(tfSecond, &offset, smFromBeginning);
    }

    if (!m_player->Play()) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }
}

void CDPlayer::pause()
{
    if (!m_player->Pause()) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }
}

void CDPlayer::resume()
{
    if (!m_player->Resume()) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }
}

void CDPlayer::stop()
{
    if (isPlaying()) {
        // set "aborted" notification if enabled
        if (m_notifyMessage) {
            m_notifyMessage = MCI_NOTIFY_ABORTED;
        }
    }

    if (!m_player->Stop()) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }
}

void CDPlayer::seekBegin()
{
    TStreamTime offset = {};
    if (!m_player->Seek(tfSecond, &offset, smFromBeginning)) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }
}

void CDPlayer::seekEnd()
{
    TStreamTime offset = {};
    if (!m_player->Seek(tfSecond, &offset, smFromEnd)) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }
}

void CDPlayer::seekTo(int32_t to)
{
    CDTime time;
    time.fromMciTime(to, m_timeFormat);

    TStreamTime offset = {};
    offset.sec = time.seconds;
    if (!m_player->Seek(tfSecond, &offset, smFromBeginning)) {
        throw WinMMError(m_player->GetError(), MCIERR_HARDWARE);
    }
}