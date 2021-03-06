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
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <signal.h>

#include "utils/winhdr.h"
#include "utils/threads/thread.h"
#include "utils/atomic.h"
#include "utils/threads/thread_delegate.h"
#include "utils/logger.h"

const size_t THREAD_NAME_SIZE = 15;

namespace threads {

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

void sleep(uint32_t ms) {
#ifdef SDL_CPP11
  std::chrono::microseconds sleep_interval_mcsec(ms * 1000);
  std::this_thread::sleep_for(std::chrono::microseconds(sleep_interval_mcsec));
#else
  Sleep(ms);
#endif
}

/* Parameter is not actual for Windows platform */
size_t Thread::kMinStackSize = 0;

void Thread::cleanup(void* arg) {
  LOGGER_AUTO_TRACE(logger_);
  Thread* thread = static_cast<Thread*>(arg);
  thread->isThreadRunning_ = false;
  thread->state_cond_.Broadcast();
}

void* Thread::threadFunc(void* arg) {
  // 0 - state_lock unlocked
  //     stopped   = 0
  //     running   = 0
  //     finalized = 0
  // 4 - state_lock unlocked
  //     stopped = 1
  //     running = 1
  //     finalized = 0
  // 5 - state_lock unlocked
  //     stopped = 1
  //     running = 1
  //     finalized = 1
  LOGGER_DEBUG(logger_,
               "Thread #" << GetCurrentThreadId() << " started successfully");

  threads::Thread* thread = static_cast<Thread*>(arg);
  DCHECK(thread);

  thread->state_lock_.Acquire();
  thread->state_cond_.Broadcast();

  while (!thread->finalized_) {
    LOGGER_DEBUG(logger_, "Thread #" << GetCurrentThreadId() << " iteration");
    thread->run_cond_.Wait(thread->state_lock_);
    LOGGER_DEBUG(logger_,
                 "Thread #" << GetCurrentThreadId() << " execute. "
                            << "stopped_ = " << thread->stopped_
                            << "; finalized_ = " << thread->finalized_);
    if (!thread->stopped_ && !thread->finalized_) {
      thread->isThreadRunning_ = true;

      thread->state_lock_.Release();
      thread->delegate_->threadMain();
      thread->state_lock_.Acquire();

      thread->isThreadRunning_ = false;
    }
    thread->state_cond_.Broadcast();
    LOGGER_DEBUG(logger_,
                 "Thread #" << GetCurrentThreadId() << " finished iteration");
  }

  thread->state_lock_.Release();

  LOGGER_DEBUG(logger_,
               "Thread #" << GetCurrentThreadId() << " exited successfully");
  return NULL;
}

void Thread::SetNameForId(const PlatformThreadHandle& thread_id,
                          std::string name) {}

Thread::Thread(const char* name, ThreadDelegate* delegate)
    : name_(name ? name : "undefined")
    , delegate_(delegate)
    , handle_(NULL)
    , thread_options_()
    , isThreadRunning_(0)
    , stopped_(false)
    , finalized_(false)
    , thread_created_(false) {}

bool Thread::start() {
  return start(thread_options_);
}

PlatformThreadHandle Thread::CurrentId() {
  return GetCurrentThread();
}

bool Thread::start(const ThreadOptions& options) {
  LOGGER_AUTO_TRACE(logger_);

  sync_primitives::AutoLock auto_lock(state_lock_);
  // 1 - state_lock locked
  //     stopped = 0
  //     running = 0

  if (!delegate_) {
    LOGGER_ERROR(logger_,
                 "Cannot start thread " << name_ << ": delegate is NULL");
    // 0 - state_lock unlocked
    return false;
  }

  if (isThreadRunning_) {
    LOGGER_TRACE(logger_,
                 "EXIT thread " << name_ << " #" << handle_
                                << " is already running");
    return true;
  }

  thread_options_ = options;

  if (!thread_created_) {
    // state_lock 1
    handle_ = ::CreateThread(
        NULL, stack_size(), (LPTHREAD_START_ROUTINE)&threadFunc, this, 0, NULL);
    if (NULL != handle_) {
      LOGGER_DEBUG(logger_, "Created thread: " << name_);
      // state_lock 0
      // possible concurrencies: stop and threadFunc
      state_cond_.Wait(auto_lock);
      thread_created_ = true;
    } else {
      LOGGER_ERROR(logger_, "Couldn't create thread " << name_);
    }
  }
  stopped_ = false;
  run_cond_.NotifyOne();
  LOGGER_DEBUG(logger_, "Thread " << name_ << " #" << handle_ << " started");
  return NULL != handle_;
}

void Thread::stop() {
  LOGGER_AUTO_TRACE(logger_);
  sync_primitives::AutoLock auto_lock(state_lock_);

  // We should check thread exit code for kThreadCancelledExitCode
  // because we need to perform cleanup in case thread has been terminated.
  // If thread is still running - usual sequence with exitThreadMain will be
  // used
  DWORD exit_code;
  if (0 != GetExitCodeThread(handle_, &exit_code) &&
      kThreadCancelledExitCode == exit_code) {
    cleanup(static_cast<void*>(this));
  }

  stopped_ = true;
  LOGGER_DEBUG(logger_,
               "Stopping thread #" << handle_ << " \"" << name_ << " \"");

  if (delegate_ && isThreadRunning_) {
    delegate_->exitThreadMain();
  }

  LOGGER_DEBUG(logger_,
               "Stopped thread #" << handle_ << " \"" << name_ << " \"");
}

void Thread::join() {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK(GetCurrentThread() != handle_);

  stop();

  sync_primitives::AutoLock auto_lock(state_lock_);
  run_cond_.NotifyOne();
  if (isThreadRunning_) {
    if (GetCurrentThread() != handle_) {
      state_cond_.Wait(auto_lock);
    }
  }
}

Thread::~Thread() {
  finalized_ = true;
  stopped_ = true;
  join();

  if (thread_options_.is_joinable()) {
    WaitForSingleObject(handle_, INFINITE);
  }
  CloseHandle(handle_);
}

Thread* CreateThread(const char* name, ThreadDelegate* delegate) {
  Thread* thread = new Thread(name, delegate);
  delegate->set_thread(thread);
  return thread;
}

void DeleteThread(Thread* thread) {
  delete thread;
}

}  // namespace threads
