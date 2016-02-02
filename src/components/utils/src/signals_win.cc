/*
 * Copyright (c) 2014, Ford Motor Company
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
#include "utils/signals.h"
#include "utils/logger.h"

CREATE_LOGGERPTR_GLOBAL(logger_, "Util")

namespace {
HANDLE signal_event = NULL;

BOOL CALLBACK sig_handler(const DWORD dwCtrlType) {
  switch (dwCtrlType) {
    case CTRL_C_EVENT:
      return ::utils::handle_event(signal_event,
                                   "CTRL_C_EVENT has been caught");
    case CTRL_BREAK_EVENT:
      return ::utils::handle_event(signal_event,
                                   "CTRL_BREAK_EVENT has been caught");
    case CTRL_CLOSE_EVENT:
      return ::utils::handle_event(signal_event,
                                   "CTRL_CLOSE_EVENT has been caught");
    case CTRL_LOGOFF_EVENT:
      return ::utils::handle_event(signal_event,
                                   "CTRL_LOGOFF_EVENT has been caught");
    case CTRL_SHUTDOWN_EVENT:
      return ::utils::handle_event(signal_event,
                                   "CTRL_SHUTDOWN_EVENT has been caught");
  }
  return FALSE;
}
}  //  namespace

namespace utils {

LONG WINAPI handleException(LPEXCEPTION_POINTERS p) {
  LOG4CXX_ERROR(logger_,
                "Illegal storage access(SIGSEGV) signal has been caught");
  return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI handle_event(HANDLE& signal_event, const char* log_event_name) {
  LOG4CXX_INFO(logger_, log_event_name);
  SetEvent(signal_event);
  return TRUE;
}

void WaitForSdlObject() {
  if (signal_event) {
    WaitForSingleObject(signal_event, INFINITE);
  } else {
    LOG4CXX_FATAL(logger_, "Create system event error");
  }
}

void CreateSdlEvent() {
  signal_event = CreateEvent(NULL, true, false, "SignalEvent");
}

bool SubscribeToTerminationSignals() {
  SetConsoleCtrlHandler(&sig_handler, TRUE);
  SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)&handleException);
  return true;
}
}  //  namespace utils
