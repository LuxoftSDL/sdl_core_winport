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

#include "utils/rwlock.h"
#include "utils/logger.h"
#include "QReadWriteLock"

namespace sync_primitives {

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

RWLock::RWLock() {
  rwlock_ = new QReadWriteLock;
  if (!rwlock_) {
    LOG4CXX_ERROR(logger_, "Failed to initialize rwlock");
  }
}

RWLock::~RWLock() {

}

void RWLock::AcquireForReading() {
  rwlock_->lockForRead();
}

bool RWLock::TryAcquireForReading() {
  return rwlock_->tryLockForRead();

}

void RWLock::AcquireForWriting() {
  rwlock_->lockForWrite();
}

bool RWLock::TryAcquireForWriting() {
  return rwlock_->tryLockForWrite();
}

 void RWLock::ReleaseForReading(){
  rwlock_->unlock();
 }

 void RWLock::ReleaseForWriting(){
  rwlock_->unlock();
 }

}  // namespace sync_primitives
