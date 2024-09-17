#ifndef ASTRASIM_COMMON_PROTOBUFUTILS_HH
#define ASTRASIM_COMMON_PROTOBUFUTILS_HH

#include <iostream>

namespace AstraSim {
namespace Utils {

class ProtobufUtils {
 public:
  static bool readVarint32(std::istream& f, uint32_t& value);

  template <typename T>
  static bool readMessage(std::istream& f, T& msg);

  static bool writeVarint32(std::ostream& f, uint32_t value);

  template <typename T>
  static bool writeMessage(std::ostream& f, const T& msg);
};

} // namespace Utils
} // namespace AstraSim

#endif /*ASTRASIM_COMMON_PROTOBUFUTILS_HH*/
