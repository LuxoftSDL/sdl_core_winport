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
#include "utils/socket.h"

#include <QtNetwork>

#include "utils/macro.h"
#include "utils/pimpl_impl.h"
#include "utils/socket_utils.h"

CREATE_LOGGERPTR_GLOBAL(logger_, "Utils")

namespace {

inline QHostAddress FromHostAddress(const utils::HostAddress& address) {
  return QHostAddress(address.ToIp4Address(true));
}

inline utils::HostAddress ToHostAddress(const QHostAddress& address) {
  return utils::HostAddress(address.toIPv4Address(), true);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// utils::TcpSocketConnection::Impl
////////////////////////////////////////////////////////////////////////////////

class utils::TcpSocketConnection::Impl {
 public:
  Impl();

  explicit Impl(QTcpSocket* tcp_socket);

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
  QTcpSocket* tcp_socket_;
};

utils::TcpSocketConnection::Impl::Impl() : tcp_socket_(NULL) {}

utils::TcpSocketConnection::Impl::Impl(QTcpSocket* tcp_socket)
    : tcp_socket_(tcp_socket) {}

utils::TcpSocketConnection::Impl::~Impl() {
  Close();
}

bool utils::TcpSocketConnection::Impl::Send(const char* buffer,
                                            const std::size_t size,
                                            std::size_t& bytes_written) {
  bytes_written = 0;
  LOG4CXX_AUTO_TRACE(logger_);
  if (!IsValid()) {
    LOG4CXX_WARN(logger_, "Cannot send. Socket is not valid.");
    return false;
  }

  QByteArray data(buffer, size);
  qint64 written = tcp_socket_->write(data);
  if (written != -1) {
    tcp_socket_->flush();
    tcp_socket_->waitForBytesWritten();
  } else {
    LOG4CXX_WARN(
        logger_,
        "Failed to send: " << tcp_socket_->errorString().toStdString());
    return false;
  }
  bytes_written = static_cast<std::size_t>(written);
  return true;
}

bool utils::TcpSocketConnection::Impl::Close() {
  LOG4CXX_AUTO_TRACE(logger_);
  if (!IsValid()) {
    LOG4CXX_DEBUG(logger_, "Not valid. Exit Close");
    return true;
  }
  tcp_socket_->close();
  delete tcp_socket_;
  tcp_socket_ = NULL;
  return true;
}

bool utils::TcpSocketConnection::Impl::IsValid() const {
  return tcp_socket_ != NULL;
}

void utils::TcpSocketConnection::Impl::EnableKeepalive() {
  utils::EnableKeepalive(
      GetNativeHandle(), kKeepAliveTimeSec, kKeepAliveIntervalSec);
}

int utils::TcpSocketConnection::Impl::GetNativeHandle() {
  if (!IsValid()) {
    return -1;
  }
  return tcp_socket_->socketDescriptor();
}

utils::HostAddress utils::TcpSocketConnection::Impl::GetAddress() const {
  if (!IsValid()) {
    return HostAddress(SpecialAddress::Any);
  }
  return ToHostAddress(tcp_socket_->peerAddress());
}

uint16_t utils::TcpSocketConnection::Impl::GetPort() const {
  if (!IsValid()) {
    return 0u;
  }
  return tcp_socket_->peerPort();
}

bool utils::TcpSocketConnection::Impl::Connect(const HostAddress& address,
                                               const uint16_t port) {
  if (IsValid()) {
    Close();
  }
  tcp_socket_ = new QTcpSocket();
  tcp_socket_->connectToHost(FromHostAddress(address), port);
  return tcp_socket_->waitForConnected();
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
                                      std::size_t size,
                                      std::size_t& bytes_written) {
  return impl_->Send(buffer, size, bytes_written);
}

bool utils::TcpSocketConnection::Close() {
  LOG4CXX_AUTO_TRACE(logger_);
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
  QTcpServer* server_socket_;
};

utils::TcpServerSocket::Impl::Impl() : server_socket_(NULL) {}

utils::TcpServerSocket::Impl::~Impl() {
  LOG4CXX_AUTO_TRACE(logger_);
  Close();
  delete server_socket_;
}

bool utils::TcpServerSocket::Impl::IsListening() const {
  return server_socket_ && server_socket_->isListening();
}

bool utils::TcpServerSocket::Impl::Close() {
  server_socket_->close();
  delete server_socket_;
  server_socket_ = NULL;
  return true;
}

bool utils::TcpServerSocket::Impl::Listen(const HostAddress& address,
                                          const uint16_t port,
                                          const int backlog) {
  LOG4CXX_AUTO_TRACE(logger_);
  LOG4CXX_DEBUG(logger_,
                "Start listening on " << address.ToString() << ":" << port);

  if (server_socket_) {
    LOG4CXX_WARN(logger_, "Cannot listen. server_socket_ is null");
    return false;
  }

  server_socket_ = new QTcpServer();

  server_socket_->setMaxPendingConnections(backlog);

  LOG4CXX_DEBUG(logger_,
                "Start listening on " << address.ToString() << ":" << port);

  if (!server_socket_->listen(FromHostAddress(address), port)) {
    LOG4CXX_WARN(
        logger_,
        "Failed to listen on " << address.ToString() << ":" << port
                               << ". Error: "
                               << server_socket_->errorString().toStdString());
    return false;
  }

  LOG4CXX_DEBUG(logger_, "Listening on " << address.ToString() << ":" << port);
  return true;
}

utils::TcpSocketConnection utils::TcpServerSocket::Impl::Accept() {
  LOG4CXX_AUTO_TRACE(logger_);
  bool waited = server_socket_->waitForNewConnection(-1);
  if (!waited) {
    LOG4CXX_WARN(logger_,
                 "Failed to wait for the new connection: "
                     << server_socket_->errorString().toStdString());
    return utils::TcpSocketConnection();
  }

  QTcpSocket* client_connection = server_socket_->nextPendingConnection();
  if (!client_connection) {
    LOG4CXX_WARN(logger_,
                 "Failed to get new connection: "
                     << server_socket_->errorString().toStdString());
    return utils::TcpSocketConnection();
  }
  LOG4CXX_DEBUG(logger_,
                "Accepted new client connection "
                    << client_connection->peerAddress().toString().toStdString()
                    << ":"
                    << client_connection->peerPort());
  return TcpSocketConnection(new TcpSocketConnection::Impl(client_connection));
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
