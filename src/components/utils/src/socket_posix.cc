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
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#include "utils/socket.h"

namespace utils {

class Socket::Impl {
 public:
  Impl();
  explicit Impl(int socket);
  ~Impl();

  Impl(Impl& rh);
  Impl& operator=(Impl& rh);

  bool Valid() const;

  bool Create(int af, int type, int protocol);
  bool Close();

  bool SetOptions(
    int level, int optname, const char* optval, size_t optlen);

  bool Bind(
    const struct sockaddr* addr, size_t addrlen);

  bool Listen(int backlog);

  Socket Accept(
    struct sockaddr* addr, size_t* addrlen);

  ssize_t Send(
    const char* buf, size_t length, int flags);

 private:
  int socket_;
};

} // namespace utils

utils::Socket::Socket()
  : impl_(new Socket::Impl()) {
}

utils::Socket::~Socket() {
  delete impl_;
}

utils::Socket::Socket(Socket& rh) {
  impl_ = new Socket::Impl();
  *impl_ = *rh.impl_;
}

utils::Socket& utils::Socket::operator=(Socket& rh) {
  if (this != &rh) {
    Socket tmp(rh);
    this->Swap(tmp);
  }
  return *this;
}

bool utils::Socket::Valid() const {
  return impl_->Valid();
}

bool utils::Socket::Create(int af, int type, int protocol) {
  return impl_->Create(af, type, protocol);
}

bool utils::Socket::Close() {
  return impl_->Close();
}

bool utils::Socket::SetOptions(
  int level, int optname, const char* optval, size_t optlen) {
  return impl_->SetOptions(level, optname, optval, optlen);
}

bool utils::Socket::Bind(
  const struct sockaddr* addr, size_t addrlen) {
  return impl_->Bind(addr, addrlen);
}

bool utils::Socket::Listen(int backlog) {
  return impl_->Listen(backlog);
}

utils::Socket utils::Socket::Accept(
  struct sockaddr* addr, size_t* addrlen) {
  return impl_->Accept(addr, addrlen);
}

ssize_t utils::Socket::Send(
  const char* buf, size_t length, int flags) {
  return impl_->Send(buf, length, flags);
}

utils::Socket::Socket(Socket::Impl* impl)
  : impl_(impl) {
}

void utils::Socket::Swap(Socket& rh) {
  std::swap(this->impl_, rh.impl_);
}

utils::Socket::Impl::Impl(): socket_(NULL) {
}

utils::Socket::Impl::Impl(int socket)
  : socket_(socket <= 0 ? NULL : socket) {
}

utils::Socket::Impl::~Impl() {
  Close();
}

utils::Socket::Impl::Impl(Impl& rh)
  : socket_(rh.socket_) {
  rh.socket_ = NULL;
}

utils::Socket::Impl& utils::Socket::Impl::operator=(Impl& rh) {
  Close();
  socket_ = rh.socket_;
  rh.socket_ = NULL;
  return *this;
}

bool utils::Socket::Impl::Valid() const {
  return socket_ != NULL;
}

bool utils::Socket::Impl::Create(int af, int type, int protocol) {
  if (!Close()) {
    return false;
  }
  socket_ = socket(af, type, protocol);
  if (-1 == socket_) {
    socket_ = NULL;
    return false;
  }
  return true;
}

bool utils::Socket::Impl::Close() {
  if (NULL == socket_) {
    return true;
  }
  if (-1 != close(socket_)) {
    return false;
  }
  socket_ = NULL;
  return true;
}

bool utils::Socket::Impl::SetOptions(
  int level, int optname, const char* optval, size_t optlen) {
  if (NULL == socket_ || -1 == setsockopt(
      socket_, level, optname, (const char*)&optval, optlen)) {
    return false;
  }
  return true;
}

bool utils::Socket::Impl::Bind(
  const struct sockaddr* addr, size_t addrlen) {
  if (NULL == socket_ || -1 == bind(socket_, addr, addrlen)) {
    return false;
  }
  return true;
}

bool utils::Socket::Impl::Listen(int backlog) {
  if (NULL == socket_ || -1 == listen(socket_, backlog)) {
    return false;
  }
  return true;
}

utils::Socket utils::Socket::Impl::Accept(
  struct sockaddr* addr, size_t* addrlen) {
  return utils::Socket(
    new utils::Socket::Impl(accept(socket_, addr, (int*)addrlen)));
}

ssize_t utils::Socket::Impl::Send(
  const char* buf, size_t length, int flags) {
  if (NULL == socket_) {
    return -1;
  }
  return send(socket_, buf, length, flags);
}
