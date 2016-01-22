/*
 * Copyright (c) 2015-2016, Ford Motor Company
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

#include "utils/rwlock.h"
#include "utils/logger.h"
#include <QReadWriteLock>

namespace sync_primitives {

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

class RWLock::Impl {
 public:
  Impl();
  ~Impl();

  void AcquireForReading();
  bool TryAcquireForReading();
  void AcquireForWriting();
  bool TryAcquireForWriting();
  void ReleaseForReading();
  void ReleaseForWriting();

 private:
  QReadWriteLock* rwlock_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

}  // namespace sync_primitives

sync_primitives::RWLock::RWLock() : impl_(new RWLock::Impl) {}

sync_primitives::RWLock::~RWLock() {
  delete impl_;
}

void sync_primitives::RWLock::AcquireForReading() {
  impl_->AcquireForReading();
}

bool sync_primitives::RWLock::TryAcquireForReading() {
  return impl_->TryAcquireForReading();
}

void sync_primitives::RWLock::AcquireForWriting() {
  impl_->AcquireForWriting();
}

bool sync_primitives::RWLock::TryAcquireForWriting() {
  return impl_->TryAcquireForWriting();
}

void sync_primitives::RWLock::ReleaseForReading() {
  impl_->ReleaseForReading();
}

void sync_primitives::RWLock::ReleaseForWriting() {
  impl_->ReleaseForWriting();
}

sync_primitives::RWLock::Impl::Impl() {
  rwlock_ = new QReadWriteLock;
}

sync_primitives::RWLock::Impl::~Impl() {
  delete rwlock_;
  rwlock_ = NULL;
}

void sync_primitives::RWLock::Impl::AcquireForReading() {
  rwlock_->lockForRead();
}

bool sync_primitives::RWLock::Impl::TryAcquireForReading() {
  if (!rwlock_->tryLockForRead()) {
    LOG4CXX_WARN(logger_, "Failed to acquire rwlock for reading");
    return false;
  }
  return true;
}

void sync_primitives::RWLock::Impl::AcquireForWriting() {
  rwlock_->lockForWrite();
}

bool sync_primitives::RWLock::Impl::TryAcquireForWriting() {
  if (!rwlock_->tryLockForWrite()) {
    LOG4CXX_WARN(logger_, "Failed to acquire rwlock for writing");
    return false;
  }
  return true;
}

void sync_primitives::RWLock::Impl::ReleaseForReading() {
  rwlock_->unlock();
}

void sync_primitives::RWLock::Impl::ReleaseForWriting() {
  rwlock_->unlock();
}