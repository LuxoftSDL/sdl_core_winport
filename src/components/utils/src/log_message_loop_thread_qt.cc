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
#include <string>
#include <cstdint>

#include <QtDebug>

#include "utils/macro.h"
#include "utils/log_message_loop_thread.h"

namespace logger {

void LogMessageHandler::Handle(const LogMessage message) {
  QMessageLogger qlogger(
    message.file.c_str(),
    message.line,
    NULL,
    message.logger_name.c_str());

  void (QMessageLogger:: * log_func) (const char*, ...) const = 0;

  std::string type_str;
  switch (message.level) {
    case 0: {
      // Qt doesn't have the trace method
      log_func = &QMessageLogger::debug;
      type_str = "TRACE";
      break;
    }
    case 1: {
      log_func = &QMessageLogger::debug;
      type_str = "DEBUG";
      break;
    }
    case 2: {
      log_func = &QMessageLogger::info;
      type_str = "INFO";
      break;
    }
    case 3: {
      log_func = &QMessageLogger::warning;
      type_str = "WARN";
      break;
    }
    case 4: {
      // Qt doesn't have the error method
      log_func = &QMessageLogger::critical;
      type_str = "ERROR";
      break;
    }
    case 5: {
      log_func = &QMessageLogger::fatal;
      type_str = "FATAL";
      break;
    }
    default: {
      assert(false && "Unsupported log level");
    }
  }

  // TODO: (malirod) Don't format manually but use QT_MESSAGE_PATTERN or qSetMessagePattern.
  // Unresolved problem: message is written in the separate thread, thus thread id in the log
  // will be the same for all messages. So the question is next, how to inject correct thread id
  // to the qlogger.
  (qlogger.*log_func)(
    "%5s [%s][%d][%s] %s",
    type_str.c_str(),
    message.time.toString("yyyy:MM:dd hh:mm:ss.zzz").toStdString().c_str(),
    message.thread_id,
    message.logger_name.c_str(),
    message.entry.c_str());
}

} // namespace logger
