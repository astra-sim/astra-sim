#include "astra-sim/common/ProtobufUtils.hh"
#include <iostream>
#include <memory>

#include "extern/graph_frontend/chakra/src/feeder_v2/raw_et_operator.h"

using namespace AstraSim::Utils;

bool ProtobufUtils::readVarint32(std::istream& f, uint32_t& value) {
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

bool ProtobufUtils::writeVarint32(std::ostream& f, uint32_t value) {
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
bool ProtobufUtils::readMessage(std::istream& f, T& msg) {
  constexpr size_t DEFAULT_BUFFER_SIZE = 16384;
  if (f.eof())
    return false;
  static char buffer[DEFAULT_BUFFER_SIZE];
  uint32_t size;
  if (!readVarint32(f, size))
    return false;
  char* buffer_use = buffer;
  if (size > DEFAULT_BUFFER_SIZE - 1) {
    // buffer is not large enough, use a dynamic buffer
    buffer_use = new char[size + 1];
  }
  f.read(buffer_use, size);
  buffer_use[size] = 0;
  msg.ParseFromArray(buffer_use, size);
  if (size > DEFAULT_BUFFER_SIZE - 1) {
    delete[] buffer_use;
  }
  return true;
}

template <typename T>
bool ProtobufUtils::writeMessage(std::ostream& f, const T& msg) {
  constexpr size_t DEFAULT_BUFFER_SIZE = 16384;
  static char buffer[DEFAULT_BUFFER_SIZE];
  size_t size = msg.ByteSizeLong();
  if (size > DEFAULT_BUFFER_SIZE - 1) {
    // buffer is not large enough, use a dynamic buffer
    char* buffer_use = new char[size];
    msg.SerializeToArray(buffer_use, size);
    writeVarint32(f, size);
    f.write(buffer_use, size);
    delete[] buffer_use;
  } else {
    msg.SerializeToArray(buffer, size);
    writeVarint32(f, size);
    f.write(buffer, size);
  }
  return true;
}

// specialization for chakra
// TODO: for some reason, now we need to define specialization in astrasim then
// chakra and find object to link, should have better solution
template bool ProtobufUtils::readMessage(std::istream& f, ChakraNode& msg);
template bool ProtobufUtils::readMessage(
    std::istream& f,
    ChakraProtoMsg::GlobalMetadata& msg);
template bool ProtobufUtils::writeMessage(
    std::ostream& f,
    const ChakraNode& msg);
template bool ProtobufUtils::writeMessage(
    std::ostream& f,
    const ChakraProtoMsg::GlobalMetadata& msg);
