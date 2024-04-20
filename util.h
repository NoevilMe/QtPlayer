#ifndef UTIL_H
#define UTIL_H

#ifndef __LOGGING_H_
#define __LOGGING_H_

#define SPDLOG_PREVENT_CHILD_FD

#if !defined(SPDLOG_ACTIVE_LEVEL)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

//
// enable/disable log calls at compile time according to global level.
//
// define SPDLOG_ACTIVE_LEVEL to one of those (before including spdlog.h):
// SPDLOG_LEVEL_TRACE,
// SPDLOG_LEVEL_DEBUG,
// SPDLOG_LEVEL_INFO,
// SPDLOG_LEVEL_WARN,
// SPDLOG_LEVEL_ERROR,
// SPDLOG_LEVEL_CRITICAL,
// SPDLOG_LEVEL_OFF
//
#define LOGT(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOGD(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOGI(...) SPDLOG_INFO(__VA_ARGS__)
#define LOGW(...) SPDLOG_WARN(__VA_ARGS__)
#define LOGE(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOGC(...) SPDLOG_CRITICAL(__VA_ARGS__)

#include <spdlog/spdlog.h>

namespace util {

namespace log {
using Logger = std::shared_ptr<spdlog::logger>;

// create logger sink
void Init(const std::string &log_path, int size = 1024 * 1024 * 20,
          int count = 10);

void ShowLoggerNameOrSourceLoation(bool show_logger_name);

void SetLevel(spdlog::level::level_enum level);

void SetLevel(const std::string &level);

void SetPattern(const std::string &pattern);

void EnableConsole();

// create or get looger
Logger GetLogger(const std::string &name);

} // namespace log
} // namespace util

#endif

namespace util {

class AtExit {
public:
    AtExit(std::function<void()> cb) : m_cb(std::move(cb)) {}

    ~AtExit() {
        if (m_cb)
            m_cb();
    }

private:
    std::function<void()> m_cb;
};


long long TimeMilliseconds();


} // namespace util

#endif // UTIL_H
