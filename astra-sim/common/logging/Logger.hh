// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include <astra-sim/common/logging/cprintf.hh>

namespace AstraSim {

/** Logger class */
class Logger {
  public:
    Logger();
    ~Logger() = default;

    template <typename... Args>
    void dprintf(uint64_t tick,
                 const std::string& name,
                 const char* fmt,
                 const Args&... args) {
        std::ostringstream line;
        ccprintf(line, fmt, args...);
        logOutMessage(tick, name, line.str());
    }

    template <typename... Args>
    void dprintf_error(uint64_t tick,
                       const std::string& name,
                       const char* fmt,
                       const Args&... args) {
        std::ostringstream line;
        ccprintf(line, fmt, args...);
        logErrMessage(tick, name, line.str());
    }

    template <typename... Args>
    void dprintf_nr(const char* fmt, const Args&... args) {
        std::ostringstream line;
        ccprintf(line, fmt, args...);
        logOutMessage(line.str());
    }

    template <typename... Args>
    void dprintf_error_nr(const char* fmt, const Args&... args) {
        std::ostringstream line;
        ccprintf(line, fmt, args...);
        logErrMessage(line.str());
    }

    void enable();
    bool isEnabled() {
        return m_enabled;
    }

  private:
    void logOutMessage(const std::string& message) const;
    void logOutMessage(uint64_t tick,
                       const std::string& name,
                       const std::string& message) const;
    void logErrMessage(const std::string& message) const;
    void logErrMessage(uint64_t tick,
                       const std::string& name,
                       const std::string& message) const;

    bool m_enabled;
};

/** Global logger variable */
extern Logger g_logger;

}  // end namespace AstraSim

/** Debugging print macros */

#define DPRINTF(x, ...)                                                        \
    do {                                                                       \
        if (AstraSim::g_logger.isEnabled())                                    \
            AstraSim::g_logger.dprintf(curTick(), name(), x, __VA_ARGS__);     \
    } while (0)

#define DPRINTFNR(x, ...)                                                      \
    do {                                                                       \
        if (AstraSim::g_logger.isEnabled())                                    \
            AstraSim::g_logger.dprintf_nr(x, __VA_ARGS__);                     \
    } while (0)

#define THROW_ERR(x, ...)                                                      \
    do {                                                                       \
        AstraSim::g_logger.dprintf_error(curTick(), name(), x, __VA_ARGS__);   \
        exit(1);                                                               \
    } while (0)

#define THROW_ERR_NR(x, ...)                                                   \
    do {                                                                       \
        AstraSim::g_logger.dprintf_error_nr(x, __VA_ARGS__);                   \
        exit(1);                                                               \
    } while (0)
