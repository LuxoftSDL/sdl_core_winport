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
#include "utils/socket.h"

#include <algorithm>

#include <QtNetwork>

CREATE_LOGGERPTR_GLOBAL(logger_ptr, "Utils.TcpSocket")

////////////////////////////////////////////////////////////////////////////////
/// utils::TcpSocketConnection::Impl
////////////////////////////////////////////////////////////////////////////////

class utils::TcpSocketConnection::Impl {
 public:
  Impl();

  explicit Impl(QTcpSocket* tcp_socket);

  ~Impl();

  ssize_t Send(const char* buffer, std::size_t size);

  bool Close();

  bool IsValid() const;

 private:
  QTcpSocket* tcp_socket_;
};

utils::TcpSocketConnection::Impl::Impl() : tcp_socket_(NULL) {}

utils::TcpSocketConnection::Impl::Impl(QTcpSocket* tcp_socket)
    : tcp_socket_(tcp_socket) {}

utils::TcpSocketConnection::Impl::~Impl() { Close(); }

ssize_t utils::TcpSocketConnection::Impl::Send(const char* buffer,
                                               std::size_t size) {
  LOG4CXX_AUTO_TRACE(logger_ptr);
  if (!IsValid()) {
    LOG4CXX_WARN(logger_ptr, "Cannot send. Socket is not valid.");
    return -1;
  }

  QByteArray data(buffer, size);
  ssize_t result = tcp_socket_->write(data);
  if (result != -1) {
    tcp_socket_->flush();
    tcp_socket_->waitForBytesWritten();
  } else {
    LOG4CXX_WARN(
        logger_ptr,
        "Failed to send: " << tcp_socket_->errorString().toStdString());
  }
  return result;
}

bool utils::TcpSocketConnection::Impl::Close() {
  LOG4CXX_AUTO_TRACE(logger_ptr);
  if (!IsValid()) {
    LOG4CXX_DEBUG(logger_ptr, "Not valid. Exit Close");
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

////////////////////////////////////////////////////////////////////////////////
/// utils::TcpSocketConnection
////////////////////////////////////////////////////////////////////////////////

utils::TcpSocketConnection::TcpSocketConnection() : impl_(new Impl()) {}

utils::TcpSocketConnection::TcpSocketConnection(Impl* impl) : impl_(impl) {
  DCHECK(impl_);
}

utils::TcpSocketConnection::~TcpSocketConnection() { delete impl_; }

utils::TcpSocketConnection::TcpSocketConnection(TcpSocketConnection& rhs) {
  Swap(rhs);
}

utils::TcpSocketConnection& utils::TcpSocketConnection::operator=(
    TcpSocketConnection& rhs) {
  if (this != &rhs) {
    Swap(rhs);
  }
  return *this;
}

void utils::TcpSocketConnection::Swap(TcpSocketConnection& rhs) {
  using std::swap;
  swap(impl_, rhs.impl_);
}

ssize_t utils::TcpSocketConnection::Send(const char* buffer, std::size_t size) {
  return impl_->Send(buffer, size);
}

bool utils::TcpSocketConnection::Close() {
  LOG4CXX_AUTO_TRACE(logger_ptr);
  return impl_->Close();
}

bool utils::TcpSocketConnection::IsValid() const { return impl_->IsValid(); }

////////////////////////////////////////////////////////////////////////////////
/// utils::ServerTcpSocket::Impl
////////////////////////////////////////////////////////////////////////////////

class utils::TcpServerSocket::Impl {
 public:
  explicit Impl();

  ~Impl();

  bool IsListening() const;

  bool Close();

  bool Listen(const std::string& address, int port, int backlog);

  TcpSocketConnection Accept();

 private:
  QTcpServer* server_socket_;
};

utils::TcpServerSocket::Impl::Impl() : server_socket_(NULL) {}

utils::TcpServerSocket::Impl::~Impl() {
  LOG4CXX_AUTO_TRACE(logger_ptr);
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

bool utils::TcpServerSocket::Impl::Listen(const std::string& address,
                                          int port,
                                          int backlog) {
  LOG4CXX_AUTO_TRACE(logger_ptr);
  if (server_socket_) {
    LOG4CXX_WARN(logger_ptr, "Cannot listen. server_socket_ is null");
    return false;
  }

  server_socket_ = new QTcpServer();

  server_socket_->setMaxPendingConnections(backlog);

  LOG4CXX_DEBUG(logger_ptr, "Start listening on " << address << ":" << port);

  if (!server_socket_->listen(QHostAddress(address.c_str()), port)) {
    LOG4CXX_WARN(
        logger_ptr,
        "Failed to listen on " << address << ":" << port << ". Error: "
                               << server_socket_->errorString().toStdString());
    return false;
  }

  return true;
}

utils::TcpSocketConnection utils::TcpServerSocket::Impl::Accept() {
  LOG4CXX_AUTO_TRACE(logger_ptr);
  bool waited = server_socket_->waitForNewConnection(-1);
  if (!waited) {
    LOG4CXX_WARN(logger_ptr,
                 "Failed to wait for the new connection: "
                     << server_socket_->errorString().toStdString());
    return utils::TcpSocketConnection();
  }

  QTcpSocket* client_connection = server_socket_->nextPendingConnection();
  if (!client_connection) {
    LOG4CXX_WARN(logger_ptr,
                 "Failed to get new connection: "
                     << server_socket_->errorString().toStdString());
    return utils::TcpSocketConnection();
  }
  return TcpSocketConnection(new TcpSocketConnection::Impl(client_connection));
}

////////////////////////////////////////////////////////////////////////////////
/// utils::TcpServerSocket
////////////////////////////////////////////////////////////////////////////////

utils::TcpServerSocket::TcpServerSocket()
    : impl_(new TcpServerSocket::Impl()) {}

utils::TcpServerSocket::~TcpServerSocket() { delete impl_; }

bool utils::TcpServerSocket::IsListening() const {
  return impl_->IsListening();
}

bool utils::TcpServerSocket::Close() { return impl_->Close(); }

bool utils::TcpServerSocket::Listen(const std::string& address,
                                    int port,
                                    int backlog) {
  return impl_->Listen(address, port, backlog);
}

utils::TcpSocketConnection utils::TcpServerSocket::Accept() {
  return impl_->Accept();
}
