#include "astra-sim/utils/logging.hh"

#include <iostream>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace AstraSim {

Logger::Stream::Stream(
    std::shared_ptr<spdlog::logger> logger,
    spdlog::level_t level) {
  this->logger = logger;
  this->level = static_cast<int>(level);
}

Logger::Stream::Stream() {
  this->logger = nullptr;
  this->level = -1;
}

void Logger::Stream::flush(void) {
  switch (this->level) {
    case spdlog::level::critical:
      this->logger->critical(this->buffer.str());
      break;
    case spdlog::level::warn:
      this->logger->warn(this->buffer.str());
      break;
    case spdlog::level::info:
      this->logger->info(this->buffer.str());
      break;
    case spdlog::level::debug:
      this->logger->debug(this->buffer.str());
      break;
    case spdlog::level::trace:
      this->logger->trace(this->buffer.str());
      break;
    default:
      assert(false);
      break;
  }
  this->buffer.str("");
}

template <typename T>
Logger::Stream& Logger::Stream::operator<<(T payload) {
  if (payload == std::endl)
    this->flush();
  else
    this->buffer << payload;
}

std::shared_ptr<Logger> Logger::getLogger(const std::string& loggerName) {
  auto it = Logger::loggers.find(loggerName);
  if (it != Logger::loggers.end())
    return it->second;

  std::shared_ptr<Logger> logger = std::make_shared<Logger>(loggerName);
  Logger::loggers.insert(
      std::pair<std::string, std::shared_ptr<Logger>>(loggerName, logger));
  return logger;
}

Logger::Logger(const std::string& loggerName) {
  std::shared_ptr<spdlog::logger> spdlogLogger =
      Logger::createSpdlogLogger(loggerName);
  this->spdlogLogger = spdlogLogger;
  this->criticle.logger = this->spdlogLogger;
  this->criticle.level = spdlog::level::critical;
  this->warn.logger = this->spdlogLogger;
  this->warn.level = spdlog::level::warn;
  this->info.logger = this->spdlogLogger;
  this->info.level = spdlog::level::info;
  this->debug.logger = this->spdlogLogger;
  this->debug.level = spdlog::level::debug;
  this->trace.logger = this->spdlogLogger;
  this->trace.level = spdlog::level::trace;
}

std::shared_ptr<spdlog::logger> Logger::createSpdlogLogger(
    const std::string& loggerName) {
  assert(spdlog::get(loggerName) == nullptr);
  if (Logger::sinks.size() == 0) {
    Logger::initSinks();
  }
  std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>(
      loggerName, std::begin(Logger::sinks), std::end(Logger::sinks));
  logger->set_level(spdlog::level::trace);
  //   spdlog::register_logger(logger);
  return logger;
}

void Logger::initSinks() {
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
