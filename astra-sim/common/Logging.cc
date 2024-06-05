#include "astra-sim/common/Logging.hh"

namespace AstraSim {

std::unordered_set<spdlog::sink_ptr> LoggerFactory::default_sinks;

std::shared_ptr<spdlog::logger> LoggerFactory::get_logger(
    const std::string& logger_name) {
  auto logger = spdlog::get(logger_name);
  if (logger == nullptr) {
    logger = spdlog::create_async<spdlog::sinks::null_sink_mt>(logger_name);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::info);
    // spdlog::register_logger(logger);
  }
  auto& logger_sinks = logger->sinks();
  for (auto sink : default_sinks)
    if (std::find(logger_sinks.begin(), logger_sinks.end(), sink) ==
        logger_sinks.end())
      logger_sinks.push_back(sink);
  return logger;
}

void LoggerFactory::init(const std::string& log_config_path) {
  if (log_config_path != "empty") {
    spdlog_setup::from_file(log_config_path);
  }
  init_default_components();
}

void LoggerFactory::shutdown(void) {
  default_sinks.clear();
  spdlog::drop_all();
  spdlog::shutdown();
}

void LoggerFactory::init_default_components() {
  auto sink_color_console =
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  sink_color_console->set_level(spdlog::level::info);
  default_sinks.insert(sink_color_console);

  auto sink_rotate_out = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      "log/log.log", 1024 * 1024 * 10, 10);
  sink_rotate_out->set_level(spdlog::level::debug);
  default_sinks.insert(sink_rotate_out);

  auto sink_rotate_err = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      "log/err.log", 1024 * 1024 * 10, 10);
  sink_rotate_err->set_level(spdlog::level::err);
  default_sinks.insert(sink_rotate_err);

  spdlog::init_thread_pool(8192, 1);
  spdlog::set_pattern("[%Y-%m-%dT%T%z] [%L] <%n>: %v");
}

} // namespace AstraSim
