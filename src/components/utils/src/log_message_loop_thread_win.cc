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
#include <sstream>
#include <cstdint>

#include "utils/winhdr.h"
#include "utils/macro.h"
#include "utils/log_message_loop_thread.h"
#include "utils/file_system.h"

namespace logger {

void LogMessageHandler::Handle(const LogMessage message) {
  std::string type_str;
  switch (message.level) {
    case LOGLEVEL_TRACE: {
      type_str = "TRACE";
      break;
    }
    case LOGLEVEL_DEBUG: {
      type_str = "DEBUG";
      break;
    }
    case LOGLEVEL_INFO: {
      type_str = "INFO ";
      break;
    }
    case LOGLEVEL_WARN: {
      type_str = "WARN ";
      break;
    }
    case LOGLEVEL_ERROR: {
      type_str = "ERROR";
      break;
    }
    case LOGLEVEL_FATAL: {
      type_str = "FATAL";
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  char time_buf[15];
  _snprintf_s(time_buf, sizeof(time_buf), "%i:%i:%i:%i",
              message.time.wHour, message.time.wMinute,
              message.time.wSecond, message.time.wMilliseconds);

  std::stringstream entry;
  entry << type_str
        << " [" << time_buf << "]"
        << " [" << message.thread_id << "]"
        << " [" << message.logger << "] "
        << file_system::RetrieveFileNameFromPath(message.file_name) << ":"
        << message.line_number << " "
        << message.function_name << ": "
        << message.entry;

  // dump log string to console
  printf(entry.str().c_str());
  printf("\n");
  // dump log string to file
  if (message.output_file) {
    fprintf(message.output_file, entry.str().c_str());
    fprintf(message.output_file, "\n");
    fflush(message.output_file);
  }
}

} // namespace logger
