// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <iomanip>

#include <astra-sim/common/logging/Logger.hh>

namespace AstraSim {
Logger g_logger;
}

using namespace AstraSim;

Logger::Logger() : m_enabled(false) {}

void Logger::logOutMessage(const std::string& message) const {
    std::cout << std::setw(10) << std::right << -1 << ": [global]"
              << " " << message;
}

void Logger::logOutMessage(uint64_t tick,
                           const std::string& name,
                           const std::string& message) const {
    std::cout << std::setw(10) << std::right << tick << ": [" << name << "]"
              << " " << message;
}

void Logger::logErrMessage(uint64_t tick,
                           const std::string& name,
                           const std::string& message) const {
    std::cerr << "ERROR:" << std::setw(10) << std::right << tick << ": " << name
              << " " << message;
}

void Logger::logErrMessage(const std::string& message) const {
    std::cerr << "ERROR:" << std::setw(10) << std::right << -1 << ": [global]"
              << " " << message;
}

void Logger::enable() {
    m_enabled = true;
}
