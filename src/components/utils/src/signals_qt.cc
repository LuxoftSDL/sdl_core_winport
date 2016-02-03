/*
 * Copyright (c) 2016, Ford Motor Company
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
#include <QCoreApplication>

CREATE_LOGGERPTR_GLOBAL(logger_, "Util")

namespace {
HANDLE signal_handle = NULL;

void sigHandler(int sig) {
  switch (sig) {
    case SIGINT:
      ::utils::handleSigs(signal_handle, "SIGINT signal has been caught");
      break;
    case SIGTERM:
      ::utils::handleSigs(signal_handle, "SIGTERM signal has been caught");
      break;
    case SIGSEGV:
      ::utils::handleSigs(signal_handle, "SIGSEGV signal has been caught");
      break;
    default:
      ::utils::handleSigs(signal_handle, "Unexpected signal has been caught");
      break;
  }
}
}  //  namespace

namespace utils {

void handleSigs(HANDLE& signal_handle, const char* log_event_name) {
  LOG4CXX_INFO(logger_, log_event_name);
  QCoreApplication* const app = QCoreApplication::instance();
  if (app) {
    app->quit();
  }
}

void WaitForSdlObject() {
  QCoreApplication::instance()->processEvents();
  QCoreApplication::instance()->exec();
}

void CreateSdlEvent() {}

void SubscribeToTerminationSignals() {
  if ((signal(SIGINT, &sigHandler) == SIG_ERR) ||
      (signal(SIGTERM, &sigHandler) == SIG_ERR) ||
      (signal(SIGSEGV, &sigHandler) == SIG_ERR)) {
    LOG4CXX_FATAL(logger_, "Subscribe to system signals error");
  }
}
}  //  namespace utils
