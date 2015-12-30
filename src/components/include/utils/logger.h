/*
 * Copyright (c) 2015, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SRC_COMPONENTS_INCLUDE_UTILS_LOGGER_H_
#define SRC_COMPONENTS_INCLUDE_UTILS_LOGGER_H_

#ifdef ENABLE_LOG

#include <errno.h>
#include <string>
#include <sstream>
#include <cstdint>

#if defined(LOG4CXX_LOGGER)
#include <log4cxx/logger.h>
#include <log4cxx/spi/loggingevent.h>
#elif defined(WIN_NATIVE)
#include "utils/winhdr.h"
#elif defined(QT_PORT)
// Qt includes goes here
#else
#error Unsupported case for logging includes
#endif

namespace logger {
bool init_logger(const std::string& ini_file_name);
void deinit_logger();

bool logs_enabled();
void set_logs_enabled(bool state);
}  // namespace logger

#define INIT_LOGGER_WITH_CFG(ini_file_name) logger::init_logger(ini_file_name)

#if defined(LOG4CXX_LOGGER)
#define INIT_LOGGER() INIT_LOGGER_WITH_CFG("log4cxx.properties")
#else
// win and qt loggers don't use config file
#define INIT_LOGGER() INIT_LOGGER_WITH_CFG("")
#endif

#define DEINIT_LOGGER() logger::deinit_logger()

#define CREATE_LOGGERPTR_GLOBAL(logger_var, logger_name) \
  namespace {                                            \
  CREATE_LOGGERPTR_LOCAL(logger_var, logger_name);       \
  }

#define LOG4CXX_ERROR_WITH_ERRNO(logger_var, message)                          \
  LOG4CXX_ERROR(logger_var,                                                    \
                message << ", error code " << errno << " (" << strerror(errno) \
                        << ")")

#define LOG4CXX_WARN_WITH_ERRNO(logger_var, message)                          \
  LOG4CXX_WARN(logger_var,                                                    \
               message << ", error code " << errno << " (" << strerror(errno) \
                       << ")")

#if defined(OS_POSIX)

namespace logger {
bool push_log(log4cxx::LoggerPtr logger,
              log4cxx::LevelPtr level,
              const std::string& entry,
              log4cxx_time_t time,
              const log4cxx::spi::LocationInfo& location,
              const log4cxx::LogString& thread_name);
log4cxx_time_t time_now();
}

#define CREATE_LOGGERPTR_LOCAL(logger_var, logger_name) \
  log4cxx::LoggerPtr logger_var =                       \
      log4cxx::LoggerPtr(log4cxx::Logger::getLogger(logger_name));

#define LOG4CXX_IS_TRACE_ENABLED(logger_var) logger_var->isTraceEnabled()

#define LOG_WITH_LEVEL(logger_var, level, message)                          \
  \
do {                                                                        \
    std::stringstream accumulator;                                          \
    accumulator << message;                                                 \
    logger::push_log(logger_var,                                            \
                     level,                                                 \
                     accumulator.str(),                                     \
                     logger::time_now(),                                    \
                     LOG4CXX_LOCATION,                                      \
                     ::log4cxx::spi::LoggingEvent::getCurrentThreadName()); \
  \
}                                                                      \
  while (false)

#undef LOG4CXX_TRACE
#define LOG4CXX_TRACE(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, ::log4cxx::Level::getTrace(), message)

#define LOG4CXX_AUTO_TRACE_WITH_NAME_SPECIFIED(logger_var, auto_trace) \
  logger::AutoTrace auto_trace(logger_var, LOG4CXX_LOCATION)
#define LOG4CXX_AUTO_TRACE(logger_var)               \
  LOG4CXX_AUTO_TRACE_WITH_NAME_SPECIFIED(logger_var, \
                                         SDL_local_auto_trace_object)

#undef LOG4CXX_DEBUG
#define LOG4CXX_DEBUG(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, ::log4cxx::Level::getDebug(), message)

#undef LOG4CXX_INFO
#define LOG4CXX_INFO(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, ::log4cxx::Level::getInfo(), message)

#undef LOG4CXX_WARN
#define LOG4CXX_WARN(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, ::log4cxx::Level::getWarn(), message)

#undef LOG4CXX_ERROR
#define LOG4CXX_ERROR(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, ::log4cxx::Level::getError(), message)

#undef LOG4CXX_FATAL
#define LOG4CXX_FATAL(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, ::log4cxx::Level::getFatal(), message)

#else

namespace logger {
enum LogLevel {
  LOGLEVEL_TRACE,
  LOGLEVEL_DEBUG,
  LOGLEVEL_INFO,
  LOGLEVEL_WARN,
  LOGLEVEL_ERROR,
  LOGLEVEL_FATAL
};
}  // namespace logger

#if defined(WIN_NATIVE)

namespace logger {
bool push_log(const std::string& logger,
              LogLevel level,
              SYSTEMTIME time,
              const std::string& entry,
              unsigned long line_number,
              const char* file_name,
              const char* function_name);
SYSTEMTIME time_now();
}

#define CREATE_LOGGERPTR_LOCAL(logger_var, logger_name) \
  std::string logger_var(logger_name);

#define LOG_WITH_LEVEL(logger_var, level, message, line_number) \
  \
do {                                                            \
    std::stringstream accumulator;                              \
    accumulator << message;                                     \
    logger::push_log(logger_var,                                \
                     level,                                     \
                     logger::time_now(),                        \
                     accumulator.str(),                         \
                     line_number,                               \
                     __FILE__,                                  \
                     __FUNCTION__);                             \
  \
}                                                          \
  while (false)

#define LOG_WITH_LEVEL_EXT(                                            \
    logger_var, level, message, line_number, file_name, function_name) \
  \
do {                                                                   \
    std::stringstream accumulator;                                     \
    accumulator << message;                                            \
    logger::push_log(logger_var,                                       \
                     level,                                            \
                     logger::time_now(),                               \
                     accumulator.str(),                                \
                     line_number,                                      \
                     file_name,                                        \
                     function_name);                                   \
  \
}                                                                 \
  while (false)

namespace logger {

class AutoTrace {
 public:
  AutoTrace(const std::string& logger,
            unsigned long line_number,
            const char* file_name,
            const char* function_name)
      : logger_(logger)
      , line_number_(line_number)
      , file_name_(file_name)
      , function_name_(function_name) {
    LOG_WITH_LEVEL_EXT(logger_,
                       LOGLEVEL_TRACE,
                       "Enter",
                       line_number_,
                       file_name_,
                       function_name_);
  };

  ~AutoTrace() {
    LOG_WITH_LEVEL_EXT(logger_,
                       LOGLEVEL_TRACE,
                       "Exit",
                       line_number_,
                       file_name_,
                       function_name_);
  };

 private:
  const std::string logger_;
  const unsigned long line_number_;
  const char* file_name_;
  const char* function_name_;
};

}  // namespace logger

#define LOG4CXX_IS_TRACE_ENABLED(logger_var) true

#undef LOG4CXX_TRACE
#define LOG4CXX_TRACE(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_TRACE, message, __LINE__)

#define LOG4CXX_AUTO_TRACE(logger_var) \
  logger::AutoTrace auto_trace(logger_var, __LINE__, __FILE__, __FUNCTION__);

#undef LOG4CXX_DEBUG
#define LOG4CXX_DEBUG(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_DEBUG, message, __LINE__)

#undef LOG4CXX_INFO
#define LOG4CXX_INFO(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_INFO, message, __LINE__)

#undef LOG4CXX_WARN
#define LOG4CXX_WARN(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_WARN, message, __LINE__)

#undef LOG4CXX_ERROR
#define LOG4CXX_ERROR(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_ERROR, message, __LINE__)

#undef LOG4CXX_FATAL
#define LOG4CXX_FATAL(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_FATAL, message, __LINE__)

#elif defined(QT_PORT)  // logging macroses for the Qt case

namespace logger {
bool push_log(const std::string& logger,
              const LogLevel level,
              const std::string& entry,
              unsigned long line_number,
              const char* file_name,
              const char* function_name);
}  // namespace logger

#define CREATE_LOGGERPTR_LOCAL(logger_var, logger_name) \
  std::string logger_var(logger_name);

#define LOG4CXX_IS_TRACE_ENABLED(logger_var) false

#define LOG_WITH_LEVEL(logger_var, level, message, line_number) \
  \
do {                                                            \
    std::stringstream accumulator;                              \
    accumulator << message;                                     \
    logger::push_log(logger_var,                                \
                     level,                                     \
                     accumulator.str(),                         \
                     line_number,                               \
                     __FILE__,                                  \
                     __FUNCTION__);                             \
  \
}                                                          \
  while (false)

#undef LOG4CXX_TRACE
#define LOG4CXX_TRACE(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_TRACE, message, __LINE__)

#define LOG4CXX_AUTO_TRACE_WITH_NAME_SPECIFIED(logger_var, auto_trace)
#define LOG4CXX_AUTO_TRACE(logger_var)

#undef LOG4CXX_DEBUG
#define LOG4CXX_DEBUG(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_DEBUG, message, __LINE__)

#undef LOG4CXX_INFO
#define LOG4CXX_INFO(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_INFO, message, __LINE__)

#undef LOG4CXX_WARN
#define LOG4CXX_WARN(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_WARN, message, __LINE__)

#undef LOG4CXX_ERROR
#define LOG4CXX_ERROR(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_ERROR, message, __LINE__)

#undef LOG4CXX_FATAL
#define LOG4CXX_FATAL(logger_var, message) \
  LOG_WITH_LEVEL(logger_var, logger::LOGLEVEL_FATAL, message, __LINE__)

#endif  // QT_PORT

#endif  // OS_POSIX

#else  // ENABLE_LOG is OFF

#define CREATE_LOGGERPTR_GLOBAL(logger_var, logger_name)
#define CREATE_LOGGERPTR_LOCAL(logger_var, logger_name)
#define INIT_LOGGER(file_name)
#define DEINIT_LOGGER(file_name)
#define LOG4CXX_IS_TRACE_ENABLED(logger_var) false

#undef LOG4CXX_TRACE
#define LOG4CXX_TRACE(x, y)

#define LOG4CXX_AUTO_TRACE_WITH_NAME_SPECIFIED(logger_var, auto_trace)
#define LOG4CXX_AUTO_TRACE(logger_var)

#undef LOG4CXX_DEBUG
#define LOG4CXX_DEBUG(x, y)

#undef LOG4CXX_INFO
#define LOG4CXX_INFO(x, y)

#undef LOG4CXX_WARN
#define LOG4CXX_WARN(x, y)

#undef LOG4CXX_ERROR
#define LOG4CXX_ERROR(x, y)

#undef LOG4CXX_ERROR_WITH_ERRNO
#define LOG4CXX_ERROR_WITH_ERRNO(x, y)

#undef LOG4CXX_WARN_WITH_ERRNO
#define LOG4CXX_WARN_WITH_ERRNO(x, y)

#undef LOG4CXX_FATAL
#define LOG4CXX_FATAL(x, y)

#endif  // ENABLE_LOG

#endif  // SRC_COMPONENTS_INCLUDE_UTILS_LOGGER_H_
