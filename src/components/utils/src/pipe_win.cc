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
#include <algorithm>

#include "utils/winhdr.h"
#include "utils/pipe.h"

namespace std {

template <>
void swap<utils::Pipe>(utils::Pipe& lhs, utils::Pipe& rhs) {
  lhs.Swap(rhs);
}

}  // namespace std

namespace utils {

class Pipe::Impl {
 public:
  Impl();
  explicit Impl(HANDLE pipe);
  ~Impl();

  Impl(Impl& rh);
  Impl& operator=(Impl& rh);

  bool Valid() const;

  bool Create(const std::string& name);

  bool Open();
  bool Close();

  ssize_t Write(const char* buf, size_t length);

 private:
  HANDLE pipe_;
};

}  // namespace utils

utils::Pipe::Pipe() : impl_(new Pipe::Impl()) {}

utils::Pipe::~Pipe() {
  delete impl_;
}

utils::Pipe::Pipe(Pipe& rh) {
  impl_ = new Pipe::Impl();
  *impl_ = *rh.impl_;
}

utils::Pipe& utils::Pipe::operator=(Pipe& rh) {
  if (this != &rh) {
    Pipe tmp(rh);
    this->Swap(tmp);
  }
  return *this;
}

bool utils::Pipe::Valid() const {
  return impl_->Valid();
}

bool utils::Pipe::Create(const std::string& name) {
  return impl_->Create(name);
}

bool utils::Pipe::Open() {
  return impl_->Open();
}

bool utils::Pipe::Close() {
  return impl_->Close();
}

ssize_t utils::Pipe::Write(const char* buf, size_t length) {
  return impl_->Write(buf, length);
}

utils::Pipe::Pipe(Pipe::Impl* impl) : impl_(impl) {}

void utils::Pipe::Swap(Pipe& rh) {
  std::swap(this->impl_, rh.impl_);
}

utils::Pipe::Impl::Impl() : pipe_(NULL) {}

utils::Pipe::Impl::Impl(HANDLE pipe)
    : pipe_(INVALID_HANDLE_VALUE == pipe ? NULL : pipe) {}

utils::Pipe::Impl::~Impl() {
  Close();
}

utils::Pipe::Impl::Impl(Impl& rh) : pipe_(rh.pipe_) {
  rh.pipe_ = NULL;
}

utils::Pipe::Impl& utils::Pipe::Impl::operator=(Impl& rh) {
  Close();
  pipe_ = rh.pipe_;
  rh.pipe_ = NULL;
  return *this;
}

bool utils::Pipe::Impl::Valid() const {
  return pipe_ != NULL;
}

bool utils::Pipe::Impl::Create(const std::string& name) {
  if (!Close()) {
    return false;
  }
  pipe_ = CreateNamedPipe(TEXT(name.c_str()),
                          PIPE_ACCESS_DUPLEX,
                          PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                          1,
                          1024,
                          1024,
                          0,
                          NULL);
  if (INVALID_HANDLE_VALUE == pipe_) {
    pipe_ = NULL;
    return false;
  }
  return true;
}

bool utils::Pipe::Impl::Open() {
  if (NULL == pipe_ || 0 == ConnectNamedPipe(pipe_, NULL)) {
    return false;
  }
  return true;
}

bool utils::Pipe::Impl::Close() {
  if (NULL == pipe_) {
    return true;
  }
  if (0 == DisconnectNamedPipe(pipe_) || 0 == CloseHandle(pipe_)) {
    return false;
  }
  pipe_ = NULL;
  return true;
}

ssize_t utils::Pipe::Impl::Write(const char* buf, size_t length) {
  if (NULL == pipe_) {
    return -1;
  }
  DWORD bytes_written = 0;
  if (0 == WriteFile(pipe_, buf, length, &bytes_written, NULL)) {
    return -1;
  }
  return static_cast<size_t>(bytes_written);
}
