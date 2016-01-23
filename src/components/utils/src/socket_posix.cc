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
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#include "utils/socket.h"
#include "utils/macro.h"

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

namespace {

bool CloseSocket(int& socket) {
  LOG4CXX_AUTO_TRACE(logger_);
  if (0 == socket) {
    LOG4CXX_DEBUG(logger_,
                  "Socket " << socket << " is not valid. Skip closing.");
    return true;
  }
  if (-1 != close(socket)) {
    LOG4CXX_WARN(logger_, "Failed to close socket " << socket << ": " << errno);
    return false;
  }
  socket = NULL;
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// utils::TcpSocketConnection::Impl
////////////////////////////////////////////////////////////////////////////////

class utils::TcpSocketConnection::Impl {
 public:
  Impl();

  explicit Impl(const SOCKET tcp_socket,
                const HostAddress& address,
                const uint16_t port);

  ~Impl();

  bool Send(const char* buffer,
            const std::size_t size,
            std::size_t& bytes_written);

  bool Close();

  bool IsValid() const;

  void EnableKeepalive();

  int GetNativeHandle();

  utils::HostAddress GetAddress() const;

  uint16_t GetPort() const;

  bool Connect(const HostAddress& address, const uint16_t port);

 private:
  int tcp_socket_;

  HostAddress address_;

  uint16_t port_;
};

utils::TcpSocketConnection::Impl::Impl()
    : tcp_socket_(0), address_(), port_(0u) {}

utils::TcpSocketConnection::Impl::Impl(const SOCKET tcp_socket,
                                       const HostAddress& address,
                                       const uint16_t port)
    : tcp_socket_(tcp_socket), address_(address), port_(port) {}

utils::TcpSocketConnection::Impl::~Impl() {
  Close();
}

bool utils::TcpSocketConnection::Impl::Send(const char* buffer,
                                            const std::size_t size,
                                            std::size_t& bytes_written) {
  LOG4CXX_AUTO_TRACE(logger_);
  bytes_written = 0u;
  if (!IsValid()) {
    LOG4CXX_ERROR(logger_, "Failed to send data socket is not valid");
    return false;
  }
  const int flags = MSG_NOSIGNAL;
  int written = send(tcp_socket_, buffer, size, flags);
  if (-1 == written) {
    LOG4CXX_ERROR(logger_, "Failed to send data: " << errno);
    return false;
  }
  bytes_written = static_cast<size_t>(written);
  return true;
}

bool utils::TcpSocketConnection::Impl::Close() {
  return CloseSocket(tcp_socket_);
}

bool utils::TcpSocketConnection::Impl::IsValid() const {
  return tcp_socket_ != 0;
}

void utils::TcpSocketConnection::Impl::EnableKeepalive() {
  int yes = 1;
  int keepidle = kKeepAliveTime;
  int keepcnt = 5;
  int keepintvl = kKeepAliveInterval;
#ifdef __linux__
  int user_timeout = 7000;  // milliseconds
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
  setsockopt(
      fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &user_timeout, sizeof(user_timeout));
#elif defined(__QNX__)
  // TODO(KKolodiy): Out of order!
  const int kMidLength = 4;
  int mib[kMidLength];

  mib[0] = CTL_NET;
  mib[1] = AF_INET;
  mib[2] = IPPROTO_TCP;
  mib[3] = TCPCTL_KEEPIDLE;
  sysctl(mib, kMidLength, NULL, NULL, &keepidle, sizeof(keepidle));

  mib[0] = CTL_NET;
  mib[1] = AF_INET;
  mib[2] = IPPROTO_TCP;
  mib[3] = TCPCTL_KEEPCNT;
  sysctl(mib, kMidLength, NULL, NULL, &keepcnt, sizeof(keepcnt));

  mib[0] = CTL_NET;
  mib[1] = AF_INET;
  mib[2] = IPPROTO_TCP;
  mib[3] = TCPCTL_KEEPINTVL;
  sysctl(mib, kMidLength, NULL, NULL, &keepintvl, sizeof(keepintvl));

  struct timeval tval = {0};
  tval.tv_sec = keepidle;
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &tval, sizeof(tval));
#else
#error Unsupported platform
#endif
}

int utils::TcpSocketConnection::Impl::GetNativeHandle() {
  return tcp_socket_;
}

utils::HostAddress utils::TcpSocketConnection::Impl::GetAddress() const {
  return address_;
}

uint16_t utils::TcpSocketConnection::Impl::GetPort() const {
  return port_;
}

bool utils::TcpSocketConnection::Impl::Connect(const HostAddress& address,
                                               const uint16_t port) {
  if (IsValid()) {
    LOG4CXX_ERROR(logger_, "Already connected. Closing existing connection.");
    Close();
  }
  int client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == client_socket) {
    LOG4CXX_ERROR(logger_, "Failed to create client socket. Error: " << errno);
    return false;
  }
  sockaddr_in server_address = {0};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  server_address.sin_addr.s_addr = address.ToIp4Address();
  if (!connect(client_socket,
               reinterpret_cast<sockaddr*>(&server_address),
               sizeof(server_address)) == 0) {
    LOG4CXX_ERROR(
        logger_,
        "Failed to connect to the server " << address.ToString() << ":" << port
                                           << ". Error: "
                                           << errno);
    CloseSocket(client_socket);
    return false;
  }
  tcp_socket_ = client_socket;
  address_ = address;
  port_ = port;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// utils::TcpSocketConnection
////////////////////////////////////////////////////////////////////////////////

// Ctor&Dtor should be in the cc file
// to prevent inlining.
// Otherwise the compiler will try to inline
// and fail to find ctor of the Pimpl.
utils::TcpSocketConnection::TcpSocketConnection() {}

utils::TcpSocketConnection::~TcpSocketConnection() {}

// This must be implemented since default assign operator takes const arg
utils::TcpSocketConnection& utils::TcpSocketConnection::operator=(
    TcpSocketConnection& rhs) {
  impl_ = rhs.impl_;
  return *this;
}

utils::TcpSocketConnection::TcpSocketConnection(Impl* impl) : impl_(impl) {
  DCHECK(impl);
}

bool utils::TcpSocketConnection::Send(const char* buffer,
                                      const std::size_t size,
                                      std::size_t& bytes_written) {
  return impl_->Send(buffer, size, bytes_written);
}

bool utils::TcpSocketConnection::Close() {
  return impl_->Close();
}

bool utils::TcpSocketConnection::IsValid() const {
  return impl_->IsValid();
}

void utils::TcpSocketConnection::EnableKeepalive() {
  impl_->EnableKeepalive();
}

int utils::TcpSocketConnection::GetNativeHandle() {
  return impl_->GetNativeHandle();
}

utils::HostAddress utils::TcpSocketConnection::GetAddress() const {
  return impl_->GetAddress();
}

uint16_t utils::TcpSocketConnection::GetPort() const {
  return impl_->GetPort();
}

bool utils::TcpSocketConnection::Connect(const HostAddress& address,
                                         const uint16_t port) {
  return impl_->Connect(address, port);
}

////////////////////////////////////////////////////////////////////////////////
/// utils::ServerTcpSocket::Impl
////////////////////////////////////////////////////////////////////////////////

class utils::TcpServerSocket::Impl {
 public:
  explicit Impl();

  ~Impl();

  bool IsListening() const;

  bool Close();

  bool Listen(const HostAddress& address,
              const uint16_t port,
              const int backlog);

  TcpSocketConnection Accept();

 private:
  int server_socket_;

  bool is_listening_;
};

utils::TcpServerSocket::Impl::Impl()
    : server_socket_(NULL), is_listening_(false) {}

utils::TcpServerSocket::Impl::~Impl() {
  Close();
}

bool utils::TcpServerSocket::Impl::IsListening() const {
  return server_socket_ && is_listening_;
}

bool utils::TcpServerSocket::Impl::Close() {
  return CloseSocket(server_socket_);
}

bool utils::TcpServerSocket::Impl::Listen(const HostAddress& address,
                                          const uint16_t port,
                                          const int backlog) {
  LOG4CXX_AUTO_TRACE(logger_);
  if (IsListening()) {
    LOG4CXX_ERROR(logger_, "Cannot listen. Already listeneing.");
    return false;
  }

  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == server_socket) {
    LOG4CXX_ERROR(logger_, "Failed to create server socket: " << errno);
    return false;
  }

  char optval = 1;
  if (-1 ==
      setsockopt(
          server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
    LOG4CXX_ERROR(logger_, "Unable to set sockopt: " << errno);
    return false;
  }

  struct sockaddr_in server_address = {0};
  server_address.sin_addr.s_addr = address.ToIp4Address();
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);

  if (-1 == bind(server_socket,
                 reinterpret_cast<struct sockaddr*>(&server_address),
                 sizeof(server_address))) {
    LOG4CXX_ERROR(logger_, "Unable to bind: " << errno);
    return false;
  }

  LOG4CXX_DEBUG(logger_,
                "Start listening on " << address.ToString() << ":" << port);

  if (-1 == listen(server_socket, backlog)) {
    LOG4CXX_ERROR(logger_, "Unable to listen: " << errno);
    LOG4CXX_WARN(logger_,
                 "Failed to listen on " << address.ToString() << ":" << port
                                        << ". Error: "
                                        << errno);
    return false;
  }

  server_socket_ = server_socket;
  is_listening_ = true;
  return true;
}

utils::TcpSocketConnection utils::TcpServerSocket::Impl::Accept() {
  LOG4CXX_AUTO_TRACE(logger_);

  struct sockaddr_in client_address = {0};
  int client_address_length = sizeof(client_address);
  int client_socket = accept(server_socket_,
                             reinterpret_cast<sockaddr*>(&client_address),
                             &client_address_length);
  if (-1 == client_socket) {
    LOG4CXX_ERROR(logger_, "Failed to accept client socket: " << errno);
    return utils::TcpSocketConnection();
  }
  if (AF_INET != client_address.sin_family) {
    LOG4CXX_DEBUG(logger_,
                  "Address of the connected client is invalid. Not AF_INET.");
    CloseSocket(client_socket);
    return utils::TcpSocketConnection();
  }
  const HostAddress accepted_client_address(inet_ntoa(client_address.sin_addr));
  LOG4CXX_DEBUG(logger_,
                "Accepted new client connection "
                    << accepted_client_address.ToString()
                    << ":"
                    << client_address.sin_port);
  return TcpSocketConnection(new TcpSocketConnection::Impl(
      client_socket, accepted_client_address, client_address.sin_port));
}

////////////////////////////////////////////////////////////////////////////////
/// utils::TcpServerSocket
////////////////////////////////////////////////////////////////////////////////

// Ctor&Dtor should be in the cc file
// to prevent inlining.
// Otherwise the compiler will try to inline
// and fail to find ctor of the Pimpl.
utils::TcpServerSocket::TcpServerSocket() {}

utils::TcpServerSocket::~TcpServerSocket() {}

// This must be implemented since default assign operator takes const arg
utils::TcpServerSocket& utils::TcpServerSocket::operator=(
    TcpServerSocket& rhs) {
  impl_ = rhs.impl_;
  return *this;
}

bool utils::TcpServerSocket::IsListening() const {
  return impl_->IsListening();
}

bool utils::TcpServerSocket::Close() {
  return impl_->Close();
}

bool utils::TcpServerSocket::Listen(const HostAddress& address,
                                    const uint16_t port,
                                    const int backlog) {
  return impl_->Listen(address, port, backlog);
}

utils::TcpSocketConnection utils::TcpServerSocket::Accept() {
  return impl_->Accept();
}
