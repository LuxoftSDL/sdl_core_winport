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
#ifndef SRC_COMPONENTS_UTILS_INCLUDE_UTILS_SOCKET_H_
#define SRC_COMPONENTS_UTILS_INCLUDE_UTILS_SOCKET_H_

#include <cstdint>
#include "utils/macro.h"

#if defined(_MSC_VER)
typedef SSIZE_T ssize_t;
#endif

namespace utils {

class Socket {
 public:
  Socket();
  ~Socket();

  Socket(Socket& rh);
  Socket& operator=(Socket& rh);

  bool Valid() const;

  bool Create(int af, int type, int protocol);
  bool Close();

  bool SetOptions(
    int level, int optname,
    const char* optval, size_t optlen);

  bool Bind(
    const struct sockaddr* addr, size_t addrlen);

  bool Listen(int backlog);

  Socket Accept(
    struct sockaddr* addr, size_t* addrlen);

  ssize_t Send(
    const char* buf, size_t length, int flags);

  void Swap(Socket& rh);

 private:
  class Impl;
  explicit Socket(Impl* impl);

  Impl* impl_;
};

}  // namespace utils

namespace std {

template<>
void swap<utils::Socket>(utils::Socket& lhs, utils::Socket& rhs) {
  lhs.Swap(rhs);
}

}  // namespace std

#endif // SRC_COMPONENTS_UTILS_INCLUDE_UTILS_SOCKET_H_
