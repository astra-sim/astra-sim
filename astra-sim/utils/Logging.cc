#include "astra-sim/utils/Logging.hh"

#include <iostream>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace AstraSim {

std::vector<spdlog::sink_ptr> Logger::sinks;

std::shared_ptr<spdlog::logger> Logger::getLogger(
    const std::string& loggerName) {
  std::shared_ptr<spdlog::logger> logger = spdlog::get(loggerName);
  if (logger != nullptr)
    return logger;
  else
    return Logger::createLogger(loggerName);
}

std::shared_ptr<spdlog::logger> Logger::createLogger(
    const std::string& loggerName) {
  assert(spdlog::get(loggerName) == nullptr);
  if (Logger::sinks.size() == 0) {
    Logger::initSinks();
  }
  std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>(
      loggerName, std::begin(Logger::sinks), std::end(Logger::sinks));
  logger->set_level(spdlog::level::trace);
  spdlog::register_logger(logger);
  return logger;
}

void Logger::initSinks() {
  const std::string log_filename = "log.log";

  spdlog::sink_ptr stdoutSink =
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  spdlog::sink_ptr fileSink =
      std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          log_filename, 1024 * 1024 * 256, 3);
  fileSink->set_level(spdlog::level::trace);
  stdoutSink->set_level(spdlog::level::info);
  Logger::sinks.push_back(fileSink);
  Logger::sinks.push_back(stdoutSink);
}

} // namespace AstraSim
