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
#include <algorithm>
#include "utils/macro.h"

#if defined(_MSC_VER)
#include "utils/winhdr.h"
typedef SSIZE_T ssize_t;
#endif

namespace utils {

class TcpSocketConnection {
 public:
  class Impl;

  TcpSocketConnection();

  TcpSocketConnection(TcpSocketConnection& rhs);

  TcpSocketConnection& operator=(TcpSocketConnection& rhs);

  explicit TcpSocketConnection(Impl* impl);

  ~TcpSocketConnection();

  ssize_t Send(const char* buffer, std::size_t size);

  bool Close();

  bool IsValid() const;
 private:
  void Swap(TcpSocketConnection& rhs);

  Impl* impl_;
};

class TcpServerSocket {
 public:
  TcpServerSocket();

  ~TcpServerSocket();

  bool IsListening() const;

  bool Close();

  bool Listen(const std::string& address, int port, int backlog);

  TcpSocketConnection Accept();

 private:
  class Impl;

  Impl* impl_;
};

} // namespace utils

#endif // SRC_COMPONENTS_UTILS_INCLUDE_UTILS_SOCKET_H_
