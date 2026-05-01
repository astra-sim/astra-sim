#include "astra-sim/common/Logging.hh"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <filesystem>

namespace AstraSim {

std::unordered_set<spdlog::sink_ptr> LoggerFactory::default_sinks;
std::shared_ptr<spdlog::logger> LoggerFactory::perf_logger = nullptr;

std::shared_ptr<spdlog::logger> LoggerFactory::get_logger(
    const std::string& logger_name) {
    constexpr bool ENABLE_DEFAULT_SINK_FOR_OTHER_LOGGERS = true;
    auto logger = spdlog::get(logger_name);
    if (logger == nullptr) {
        logger = spdlog::create_async<spdlog::sinks::null_sink_mt>(logger_name);
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::info);
    }
    if constexpr (!ENABLE_DEFAULT_SINK_FOR_OTHER_LOGGERS) {
        return logger;
    }
    auto& logger_sinks = logger->sinks();
    for (auto sink : default_sinks) {
        if (std::find(logger_sinks.begin(), logger_sinks.end(), sink) ==
            logger_sinks.end()) {
            logger_sinks.push_back(sink);
        }
    }
    return logger;
}

std::shared_ptr<spdlog::logger> LoggerFactory::get_perf_logger() {
    return perf_logger;
}

void LoggerFactory::init(const std::string& log_config_path,
                         const std::string& log_path) {
    if (log_config_path != "empty") {
        spdlog_setup::from_file(log_config_path);
    }
    init_default_components(log_path);
}

void LoggerFactory::shutdown(void) {
    default_sinks.clear();
    perf_logger.reset();
    spdlog::drop_all();
    spdlog::shutdown();
}

void LoggerFactory::init_default_components(const std::string& log_path) {
    std::filesystem::path folderPath(log_path);

    if (!std::filesystem::exists(folderPath)) {
        std::filesystem::create_directory(folderPath);
    }

    spdlog::init_thread_pool(8192, 1);

    auto perf_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path + "/perf.log", 1024 * 1024 * 10, 10);
    perf_sink->set_level(spdlog::level::trace);
    perf_logger = std::make_shared<spdlog::async_logger>(
        "PerfLogger", perf_sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    perf_logger->set_level(spdlog::level::trace);
    perf_logger->flush_on(spdlog::level::info);
    perf_logger->set_pattern("%v");
    perf_logger->info("sys_id,"
                      "compute_active_ns,compute_idle_ns,"
                      "memory_read_bytes,memory_read_nums,"
                      "memory_write_bytes,memory_write_nums,"
                      "network_send_bytes,network_send_nums,"
                      "network_recv_bytes,network_recv_nums");

    auto sink_color_console =
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink_color_console->set_level(spdlog::level::info);
    default_sinks.insert(sink_color_console);

    auto sink_rotate_out =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path + "/log.log", 1024 * 1024 * 10, 10);
    sink_rotate_out->set_level(spdlog::level::debug);
    default_sinks.insert(sink_rotate_out);

    auto sink_rotate_err =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path + "/err.log", 1024 * 1024 * 10, 10);
    sink_rotate_err->set_level(spdlog::level::err);
    default_sinks.insert(sink_rotate_err);

    spdlog::set_pattern("[%Y-%m-%dT%T%z] [%L] <%n>: %v");
}

}  // namespace AstraSim
