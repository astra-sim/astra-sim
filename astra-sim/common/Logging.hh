#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include "spdlog_setup/conf.h"

namespace AstraSim {

class LoggerFactory {
 public:
  LoggerFactory() = delete;
  static std::shared_ptr<spdlog::logger> get_logger(
      const std::string& logger_name);
  static void init(const std::string& log_conf_path = "empty");
  static void shutdown(void);

 private:
  static void init_default_components();
  static std::unordered_set<spdlog::sink_ptr> default_sinks;
};

} // namespace AstraSim
