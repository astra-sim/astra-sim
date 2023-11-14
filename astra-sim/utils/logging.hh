#ifndef ASTRASIM_LOGGING
#define ASTRASIM_LOGGING

#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

const std::string log_filename = "log.log";

namespace AstraSim {

class Logger {
 public:
  std::shared_ptr<Logger> getLogger(const std::string& loggerName);
  class Stream {
   public:
    Stream();
    Stream(std::shared_ptr<spdlog::logger> logger, spdlog::level_t level);
    template <typename T>
    Stream& operator<<(T payload);
    void flush(void);
    std::stringstream buffer;
    std::shared_ptr<spdlog::logger> logger;
    spdlog::level_t level;
  };

 private:
  Logger(const std::string& loggerName);
  std::shared_ptr<spdlog::logger> spdlogLogger;
  Stream criticle;
  Stream warn;
  Stream info;
  Stream debug;
  Stream trace;

  static std::shared_ptr<spdlog::logger> createSpdlogLogger(
      const std::string& loggerName);
  static void initSinks();
  static std::vector<spdlog::sink_ptr> sinks;
  static std::map<std::string, std::shared_ptr<Logger>> loggers;
};

} // namespace AstraSim

#endif
