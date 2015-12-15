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
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "utils/logger.h"
#include "utils/log_message_loop_thread.h"

namespace {

bool is_logs_enabled = false;

const QString kLogFileName = "SmartDeviceLink.log";

// Used to pass log file to the log_output_handler
QFile* log_file_ptr = NULL;

logger::LogMessageLoopThread* message_loop_thread = NULL;

void log_output_handler(QtMsgType, const QMessageLogContext&, const QString& msg) {
  // Log to console
  fprintf(stdout, "%s\n", msg.toLocal8Bit().constData());
  // Log to file
  if (log_file_ptr) {
   QTextStream log_stream(log_file_ptr);
   log_stream << msg.toLocal8Bit().constData() << endl;
  }
}

} // namespace

bool logger::init_logger(const std::string&) {
  // TODO: (malirod) Use RAII to manage with log files
  static QFile log_file(kLogFileName);

  if (log_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    assert(!log_file_ptr);
    log_file_ptr = &log_file;
  } else {
    fprintf(stderr, "Logging initialization has failed. Failed to open log file for writing.\n");
  }

  // Set qt custom log handler
  qInstallMessageHandler(log_output_handler);

  if (!message_loop_thread) {
    message_loop_thread = new LogMessageLoopThread();
  }
  set_logs_enabled(true);
  return true;
}

void logger::deinit_logger() {
  CREATE_LOGGERPTR_LOCAL(logger_, "Logger");
  LOG4CXX_DEBUG(logger_, "Logger deinitialization");

  assert(log_file_ptr);
  if (log_file_ptr) {
    log_file_ptr ->close();
    log_file_ptr = NULL;
  }

  set_logs_enabled(false);
  delete message_loop_thread;
  message_loop_thread = NULL;
}

bool logger::logs_enabled() {
  return is_logs_enabled;
}

void logger::set_logs_enabled(bool state) {
  is_logs_enabled = state;
}

bool logger::push_log(
  const std::string& logger,
  const uint32_t level,
  const std::string& entry,
  unsigned long line_number,
  const char* file_name,
  const char* function_name) {
  if (!logs_enabled()) {
    return false;
  }

  LogMessage message = {
    logger,
    level,
    QDateTime::currentDateTime(),
    entry,
    line_number,
    file_name,
    function_name,
    reinterpret_cast<uint32_t>(QThread::currentThreadId()) };

  message_loop_thread->PostMessage(message);

  return true;
}
