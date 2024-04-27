#include "util.h"

#include <spdlog/common.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <string>

namespace util {
namespace log {

static std::string s_log_pattern = "%Y-%m-%d %T.%e %P.%t [%l] [%s:%#] %n - %v";
static std::mutex s_log_mutex;
static std::vector<spdlog::sink_ptr> s_sinks;
static spdlog::level::level_enum s_level = spdlog::level::info;

void Init(const std::string &log_path, int size, int count) {
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> log_sink =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_path, size,
                                                               count);
    s_sinks.push_back(log_sink);

    spdlog::default_logger()->sinks().clear();
    spdlog::default_logger()->sinks().push_back(log_sink);
}

void ShowLoggerNameOrSourceLoation(bool show_logger_name) {
    if (show_logger_name) {
        s_log_pattern = "%Y-%m-%d %T.%e %P.%t [%l] %n - %v";
    } else {
        s_log_pattern = "%Y-%m-%d %T.%e %P.%t [%l] [%s:%#] - %v";
    }
}

void SetLevel(spdlog::level::level_enum level) {
    s_level = level;
    // this only effects existing loggers
    spdlog::set_level(level);
}

void EnableConsole() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // console_sink->set_level(level_);
    console_sink->set_pattern(s_log_pattern);
    s_sinks.push_back(console_sink);
    spdlog::default_logger()->sinks().push_back(console_sink);
}

void SetLevel(const std::string &level) {
    auto lev = spdlog::level::from_str(level);
    SetLevel(lev);
}

void SetPattern(const std::string &pattern) { s_log_pattern = pattern; }

Logger GetLogger(const std::string &name) {

    std::unique_lock<std::mutex> lck(s_log_mutex);

    if (s_sinks.empty())
        return nullptr;

    auto logger = spdlog::get(name);
    if (!logger) {
        logger = std::make_shared<spdlog::logger>(name, s_sinks.begin(),
                                                  s_sinks.end());
        logger->set_pattern(s_log_pattern);
        logger->set_level(s_level);
        logger->flush_on(s_level);
        spdlog::register_logger(logger);
    }
    return logger;
}

} // namespace log

long long TimeMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// namespace log
} // namespace util
