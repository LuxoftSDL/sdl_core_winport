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
#if defined(WIN_NATIVE)

#include <windows.h>

#include "utils/rwlock.h"
#include "utils/logger.h"

namespace sync_primitives {

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

RWLock::RWLock()
  : status_(kNotAcquired) {
  InitializeSRWLock(&rwlock_);
}

RWLock::~RWLock() {
  if (kNotAcquired != status_) {
    LOG4CXX_ERROR(logger_, "RWLock is acquired");
  }
}

bool RWLock::AcquireForReading() {
  if (kNotAcquired != status_) {
    LOG4CXX_WARN(logger_, "RWLock is already acquired");
	return false;
  }
  AcquireSRWLockShared(&rwlock_);
  status_ = kAcquiredForReading;
  return true;
}

bool RWLock::TryAcquireForReading() {
  if (kNotAcquired != status_) {
    LOG4CXX_WARN(logger_, "RWLock is already acquired");
	return false;
  }
  if (!TryAcquireSRWLockShared(&rwlock_)) {
    LOG4CXX_WARN(logger_, "Failed to acquire rwlock for reading");
    return false;
  }
  status_ = kAcquiredForReading;
  return true;
}

bool RWLock::AcquireForWriting() {
  if (kNotAcquired != status_) {
    LOG4CXX_WARN(logger_, "RWLock is already acquired");
	return false;
  }
  AcquireSRWLockExclusive(&rwlock_);
  status_ = kAcquiredForWriting;
  return true;
}

bool RWLock::TryAcquireForWriting() {
  if (kNotAcquired != status_) {
    LOG4CXX_WARN(logger_, "RWLock is already acquired");
	return false;
  }
  if (!TryAcquireSRWLockExclusive(&rwlock_)) {
    LOG4CXX_WARN(logger_, "Failed to acquire rwlock for writing");
    return false;
  }
  status_ = kAcquiredForWriting;
  return true;
}

bool RWLock::Release() {
  if (kAcquiredForReading == status_) {
    ReleaseSRWLockShared(&rwlock_);
  } else if (kAcquiredForWriting == status_) {
    ReleaseSRWLockExclusive(&rwlock_);
  } else {
    LOG4CXX_WARN(logger_, "RWLock is not acquired");
    return false;
  }
  status_ = kNotAcquired;
  return true;
}

}  // namespace sync_primitives

#endif // WIN_NATIVE
