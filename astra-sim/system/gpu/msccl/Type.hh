// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <string>

#include <astra-sim/common/logging/Logger.hh>

namespace AstraSim {
namespace Msccl {

/**
 * BufferType represents the type of buffer used in MSCCL++
 */

enum class BufferType {
    Input = 0,
    Output = 1,
    Scratch = 2,
    Semaphore = 3,
    NumBufferTypes
};

// Get buffer type from its string representation
inline BufferType getBufferTypeFromString(const std::string& type_str) {
    if (type_str == "i") {
        return BufferType::Input;
    } else if (type_str == "o") {
        return BufferType::Output;
    } else if (type_str == "s") {
        return BufferType::Scratch;
    } else {
        THROW_ERR_NR("Invalid buffer type string: %s\n", type_str.c_str());
    }
}

// Get string representation of buffer type
inline std::string bufferTypeToString(BufferType type) {
    switch (type) {
    case BufferType::Input:
        return "Input";
    case BufferType::Output:
        return "Output";
    case BufferType::Scratch:
        return "Scratch";
    case BufferType::Semaphore:
        return "Semaphore";
    default:
        THROW_ERR_NR("Invalid buffer type: %d\n", static_cast<int>(type));
    }
}

/**
 * ChannelType represents the type of channel used in MSCCL++
 */
enum class ChannelType { Memory = 0, Switch = 1, Port = 2, NumChannelTypes };

// Get channel type from its string representation
inline ChannelType getChannelTypeFromString(const std::string& type_str) {
    if (type_str == "memory") {
        return ChannelType::Memory;
    } else if (type_str == "switch") {
        return ChannelType::Switch;
    } else if (type_str == "port") {
        return ChannelType::Port;
    } else {
        THROW_ERR_NR("Invalid channel type string: %s\n", type_str.c_str());
    }
}

// Get string representation of channel type
inline std::string channelTypeToString(ChannelType type) {
    switch (type) {
    case ChannelType::Memory:
        return "Memory";
    case ChannelType::Switch:
        return "Switch";
    case ChannelType::Port:
        return "Port";
    default:
        THROW_ERR_NR("Invalid channel type: %d\n", static_cast<int>(type));
    }
}

/**
 * Protocol represents the type of communication protocol used in MSCCL++
 */
enum class ProtocolType { Simple, LL, LL64, LL128, LL256, NumProtocols };

// Get protocol type from its string representation
inline ProtocolType getProtocolTypeFromString(const std::string& type_str) {
    if (type_str == "Simple") {
        return ProtocolType::Simple;
    } else if (type_str == "LL") {
        return ProtocolType::LL;
    } else if (type_str == "LL64") {
        return ProtocolType::LL64;
    } else if (type_str == "LL128") {
        return ProtocolType::LL128;
    } else if (type_str == "LL256") {
        return ProtocolType::LL256;
    } else {
        THROW_ERR_NR("Invalid protocol type string: %s\n", type_str.c_str());
    }
}

// Get string representation of channel type
inline std::string protocolTypeToString(ProtocolType type) {
    switch (type) {
    case ProtocolType::Simple:
        return "Simple";
    case ProtocolType::LL:
        return "LL";
    case ProtocolType::LL64:
        return "LL64";
    case ProtocolType::LL128:
        return "LL128";
    case ProtocolType::LL256:
        return "LL256";
    default:
        THROW_ERR_NR("Invalid protocol type: %d\n", static_cast<int>(type));
    }
}

}  // namespace Msccl
}  // namespace AstraSim
