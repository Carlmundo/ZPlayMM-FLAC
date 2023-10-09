#include "CDTime.hpp"

#include <Windows.h>

void CDTime::fromMciTime(int32_t mciTime, int32_t format)
{
    switch (format) {
        case MCI_FORMAT_MILLISECONDS:
            track = 0;
            seconds = mciTime / 1000;
            break;

        case MCI_FORMAT_MSF: {
            int32_t minute = MCI_MSF_MINUTE(mciTime);
            int32_t second = MCI_MSF_SECOND(mciTime);
            track = 0;
            seconds = second + minute * 60;
            break;
        }

        case MCI_FORMAT_TMSF: {
            int32_t minute = MCI_TMSF_MINUTE(mciTime);
            int32_t second = MCI_TMSF_SECOND(mciTime);
            track = MCI_TMSF_TRACK(mciTime);
            seconds = second + minute * 60;
            break;
        }
    }
}

int32_t CDTime::toMciTime(int32_t format)
{
    switch (format) {
        case MCI_FORMAT_MILLISECONDS:
            return seconds * 1000;

        case MCI_FORMAT_MSF:
            return MCI_MAKE_MSF(seconds / 60, seconds % 60, 0);

        case MCI_FORMAT_TMSF:
            return MCI_MAKE_TMSF(track, seconds / 60, seconds % 60, 0);
    }

    return 0;
}