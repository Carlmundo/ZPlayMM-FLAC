#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

class WinMMError : public std::runtime_error
{
public:
    WinMMError(int32_t errorCode);
    WinMMError(const char* message, int32_t errorCode);
    WinMMError(const std::string& message, int32_t errorCode);
    int32_t getErrorCode() const;
    const std::string& getErrorName() const;

private:
    int32_t m_errorCode;
};