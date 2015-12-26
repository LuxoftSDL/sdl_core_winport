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
#include "utils/conditional_variable.h"

#include <errno.h>
#include <time.h>

#include "utils/lock.h"
#include "utils/logger.h"
#include "QWaitCondition"

namespace {
const long kNanosecondsPerSecond = 1000000000;
const long kMillisecondsPerSecond = 1000;
const long kNanosecondsPerMillisecond = 1000000;
}

namespace sync_primitives {

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

ConditionalVariable::ConditionalVariable() {
  cond_var_ = new QWaitCondition;
  if (!cond_var_){
    LOG4CXX_ERROR(logger_, "Failed to initialize "
                            "conditional variable");
  }
}

ConditionalVariable::~ConditionalVariable() {
}

void ConditionalVariable::NotifyOne() {
  cond_var_->wakeOne();
}

void ConditionalVariable::Broadcast() {
  cond_var_->wakeAll();
}

bool ConditionalVariable::Wait(Lock& lock) {
  lock.AssertTakenAndMarkFree();
    bool wait_status = cond_var_->wait(lock.mutex_,INFINITE);
  lock.AssertFreeAndMarkTaken();
  if (!wait_status) {
    LOG4CXX_ERROR(logger_, "Failed to wait for conditional variable");
    return false;
  }
  return true;
}

bool ConditionalVariable::Wait(AutoLock& auto_lock) {
  Lock& lock = auto_lock.GetLock();
  lock.AssertTakenAndMarkFree();
  bool wait_status = cond_var_->wait(lock.mutex_,INFINITE);
  lock.AssertFreeAndMarkTaken();
  if (!wait_status) {
    LOG4CXX_ERROR(logger_, "Failed to wait for conditional variable");
    return false;
  }
  return true;
}

ConditionalVariable::WaitStatus ConditionalVariable::WaitFor(
    AutoLock& auto_lock, int32_t milliseconds){
  Lock& lock = auto_lock.GetLock();
  lock.AssertTakenAndMarkFree();
  int32_t timedwait_status = cond_var_->wait(lock.mutex_, milliseconds);
  lock.AssertFreeAndMarkTaken();
  WaitStatus wait_status = kNoTimeout;
  switch(timedwait_status) {
    case 0: {
      wait_status = kNoTimeout;
      break;
    }
    case EINTR: {
      wait_status = kNoTimeout;
      break;
    }
    case ETIMEDOUT: {
      wait_status = kTimeout;
      break;
    }
    default: {
      LOG4CXX_ERROR(logger_, "Failed to timewait for conditional variable timedwait_status: " 
        << timedwait_status);
    }
  }
  return wait_status;
}

} // namespace sync_primitives
