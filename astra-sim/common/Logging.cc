#include "astra-sim/utils/Logging.hh"

#include <iostream>

#include "spdlog/async.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace AstraSim {

std::vector<spdlog::sink_ptr> Logger::sinks;
std::vector<std::shared_ptr<spdlog::logger>> Logger::loggers;

std::shared_ptr<spdlog::logger> Logger::getLogger(
    const std::string& loggerName) {
  std::shared_ptr<spdlog::logger> logger = spdlog::get(loggerName);
  if (logger != nullptr)
    return logger;
  else
    return Logger::createLogger(loggerName);
}

void Logger::shutdown(void) {
  spdlog::drop_all();
  spdlog::shutdown();
  for (auto logger : loggers) {
    logger->sinks().clear();
  }
  Logger::sinks.clear();
  Logger::loggers.clear();
}

std::shared_ptr<spdlog::logger> Logger::createLogger(
    const std::string& loggerName) {
  assert(spdlog::get(loggerName) == nullptr);
  if (Logger::sinks.size() == 0) {
    Logger::initSinks();
  }
  std::shared_ptr<spdlog::logger> logger =
      spdlog::create_async<spdlog::sinks::null_sink_st>(loggerName);
  for (spdlog::sink_ptr sink : Logger::sinks) {
    logger->sinks().push_back(sink);
  }
  logger->set_level(spdlog::level::trace);
  logger->flush_on(spdlog::level::info);
  // spdlog::register_logger(logger);
  loggers.push_back(logger);
  return logger;
}

void Logger::initSinks() {
  const std::string log_filename = "log.log";
  spdlog::init_thread_pool(65536, 1);

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
