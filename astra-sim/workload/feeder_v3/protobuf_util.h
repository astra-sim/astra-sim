#ifndef CHAKRA_FEEDER_V3_PROTOBUF_UTIL_H
#define CHAKRA_FEEDER_V3_PROTOBUF_UTIL_H

#include <cstdint>
#include <iostream>
#include <mutex>
#include "common.h"

namespace Chakra {
namespace FeederV3 {
class ProtobufUtils {
 public:
  static bool readVarint32(std::istream& f, uint32_t& value) {
    std::unique_lock<std::mutex> lock(_mutex);
    uint8_t byte;
    value = 0;
    int8_t shift = 0;
    while (f.read(reinterpret_cast<char*>(&byte), 1)) {
      value |= (byte & 0x7f) << shift;
      if (!(byte & 0x80))
        return true;
      shift += 7;
      if (shift > 28)
        return false;
    }
    return false;
  }

  template <typename T>
  static bool readMessage(std::istream& f, T& msg) {
    std::unique_lock<std::mutex> lock(_mutex);
    if (f.eof())
      return false;
    static char buffer[DEFAULT_PROTOBUF_BUFFER_SIZE];
    uint32_t size;
    lock.unlock();
    if (!readVarint32(f, size))
      return false;
    lock.lock();
    char* buffer_use = buffer;
    if (size > DEFAULT_PROTOBUF_BUFFER_SIZE - 1) {
      // buffer is not large enough, use a dynamic buffer
      buffer_use = new char[size + 1];
    }
    f.read(buffer_use, size);
    buffer_use[size] = 0;
    msg.ParseFromArray(buffer_use, size);
    if (size > DEFAULT_PROTOBUF_BUFFER_SIZE - 1) {
      delete[] buffer_use;
    }
    return true;
  }

  static bool writeVarint32(std::ostream& f, uint32_t value) {
    std::unique_lock<std::mutex> lock(_mutex);
    uint8_t byte;
    while (value > 0x7f) {
      byte = (value & 0x7f) | 0x80;
      f.write(reinterpret_cast<char*>(&byte), 1);
      value >>= 7;
    }
    byte = value;
    f.write(reinterpret_cast<char*>(&byte), 1);
    return true;
  }

  template <typename T>
  static bool writeMessage(std::ostream& f, T& msg) {
    std::unique_lock<std::mutex> lock(_mutex);
    static char buffer[DEFAULT_PROTOBUF_BUFFER_SIZE];
    size_t size = msg.ByteSizeLong();
    if (size > DEFAULT_PROTOBUF_BUFFER_SIZE - 1) {
      // buffer is not large enough, use a dynamic buffer
      char* buffer_use = new char[size];
      msg.SerializeToArray(buffer_use, size);
      lock.unlock();
      writeVarint32(f, size);
      lock.lock();
      f.write(buffer_use, size);
      delete[] buffer_use;
    } else {
      msg.SerializeToArray(buffer, size);
      lock.unlock();
      writeVarint32(f, size);
      lock.lock();
      f.write(buffer, size);
    }
    return true;
  }

 private:
  static std::mutex _mutex;
};

std::mutex ProtobufUtils::_mutex;
} // namespace FeederV3
} // namespace Chakra
#endif
