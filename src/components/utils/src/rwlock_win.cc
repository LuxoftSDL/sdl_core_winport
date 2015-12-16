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
#include "utils/winhdr.h"
#include "utils/lock.h"
#include "utils/rwlock.h"
#include "utils/logger.h"

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

namespace sync_primitives {

class RWLock::Impl {
 public:
  Impl();
  ~Impl();

  bool AcquireForReading();
  bool TryAcquireForReading();
  bool TryAcquireForWriting();
  bool AcquireForWriting();
  bool Release();

 private:
  enum RWLockStatus { kNotAcquired, kAcquiredForReading, kAcquiredForWriting };

  RWLockStatus status_;
  Lock         status_lock_;
  SRWLOCK      rwlock_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

}  // namespace sync_primitives

sync_primitives::RWLock::RWLock()
  : impl_(new RWLock::Impl) {
}

sync_primitives::RWLock::~RWLock() {
  delete impl_;
}

bool sync_primitives::RWLock::AcquireForReading() {
  return impl_->AcquireForReading();
}

bool sync_primitives::RWLock::TryAcquireForReading() {
  return impl_->TryAcquireForReading();
}

bool sync_primitives::RWLock::AcquireForWriting() {
  return impl_->AcquireForWriting();
}

bool sync_primitives::RWLock::TryAcquireForWriting() {
  return impl_->TryAcquireForWriting();
}

bool sync_primitives::RWLock::Release() {
  return impl_->Release();
}

sync_primitives::RWLock::Impl::Impl()
  : status_(kNotAcquired) {
  InitializeSRWLock(&rwlock_);
}

sync_primitives::RWLock::Impl::~Impl() {
  sync_primitives::AutoLock lock(status_lock_);
  if (kNotAcquired != status_) {
    LOG4CXX_ERROR(logger_, "RWLock is acquired");
  }
}

bool sync_primitives::RWLock::Impl::AcquireForReading() {
  sync_primitives::AutoLock lock(status_lock_);
  AcquireSRWLockShared(&rwlock_);
  status_ = kAcquiredForReading;
  return true;
}

bool sync_primitives::RWLock::Impl::TryAcquireForReading() {
  sync_primitives::AutoLock lock(status_lock_);
  if (!TryAcquireSRWLockShared(&rwlock_)) {
    LOG4CXX_WARN(logger_, "Failed to acquire rwlock for reading");
    return false;
  }
  status_ = kAcquiredForReading;
  return true;
}

bool sync_primitives::RWLock::Impl::AcquireForWriting() {
  sync_primitives::AutoLock lock(status_lock_);
  AcquireSRWLockExclusive(&rwlock_);
  status_ = kAcquiredForWriting;
  return true;
}

bool sync_primitives::RWLock::Impl::TryAcquireForWriting() {
  sync_primitives::AutoLock lock(status_lock_);
  if (!TryAcquireSRWLockExclusive(&rwlock_)) {
    LOG4CXX_WARN(logger_, "Failed to acquire rwlock for writing");
    return false;
  }
  status_ = kAcquiredForWriting;
  return true;
}

bool sync_primitives::RWLock::Impl::Release() {
  sync_primitives::AutoLock lock(status_lock_);
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
