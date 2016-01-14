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
#include <string>
#include <algorithm>
#include <cstddef>

#include <QtNetwork>
#include "utils/pipe.h"

CREATE_LOGGERPTR_GLOBAL(logger_ptr, "Utils.QtPipe")

namespace utils {

////////////////////////////////////////////////////////////////////////////////
/// class utils::Pipe::Impl
////////////////////////////////////////////////////////////////////////////////
class Pipe::Impl {
 public:
  Impl();
  ~Impl();

  Impl& operator=(Impl& rh);

  bool isValid() const;

  bool Create(const std::string& name);

  bool Open();
  bool Close();

  ssize_t Write(const char* buf, std::size_t length);

 private:
  QLocalServer* server_socket_;
  QLocalSocket* client_socket_;
  QString name_;
};

}  // namespace utils

////////////////////////////////////////////////////////////////////////////////
/// class utils::Pipe
////////////////////////////////////////////////////////////////////////////////
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
  return impl_->isValid();
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

ssize_t utils::Pipe::Write(const char* buf, std::size_t length) {
  return impl_->Write(buf, length);
}

utils::Pipe::Pipe(Pipe::Impl* impl) : impl_(impl) {}

void utils::Pipe::Swap(Pipe& rh) {
  qSwap(this->impl_, rh.impl_);
}

utils::Pipe::Impl::Impl() : server_socket_(NULL), client_socket_(NULL) {}

utils::Pipe::Impl::~Impl() {
  Close();
}

utils::Pipe::Impl& utils::Pipe::Impl::operator=(Impl& rh) {
  Close();
  qSwap(server_socket_, rh.server_socket_);
  qSwap(client_socket_, rh.client_socket_);
  rh.name_ = "";
  return *this;
}

bool utils::Pipe::Impl::isValid() const {
  return server_socket_ != NULL;
}

bool utils::Pipe::Impl::Create(const std::string& name) {
  if (!Close()) {
    return false;
  }
  server_socket_ = new QLocalServer();
  const QString stream_path(name.c_str());
  const QStringList stream_path_list = stream_path.split(QDir::separator());
  name_ = kSDLStreamingPipeBase;
  name_.append(stream_path_list.back());
  server_socket_->setSocketOptions(QLocalServer::WorldAccessOption);
  return server_socket_->listen(name_);
}

bool utils::Pipe::Impl::Open() {
  if (!server_socket_->isListening()) {
    LOG4CXX_WARN(
        logger_ptr,
        "Failed listening: " << server_socket_->errorString().toStdString());
    return false;
  }
  if (server_socket_->waitForNewConnection(-1)) {
    client_socket_ = server_socket_->nextPendingConnection();
  }
  if (!client_socket_) {
    LOG4CXX_WARN(logger_ptr,
                 "Failed to get new connection: "
                     << server_socket_->errorString().toStdString());
    return false;
  }
  return true;
}

bool utils::Pipe::Impl::Close() {
  if (!client_socket_ && !server_socket_) {
    return true;
  }
  if (client_socket_) {
    client_socket_->disconnectFromServer();
  }
  if (server_socket_) {
    server_socket_->close();
  }
  delete client_socket_;
  client_socket_ = NULL;
  delete server_socket_;
  server_socket_ = NULL;
  name_ = "";
  return true;
}

ssize_t utils::Pipe::Impl::Write(const char* buf, std::size_t length) {
  if (!client_socket_) {
    LOG4CXX_WARN(
        logger_ptr,
        "Failed to send: " << server_socket_->errorString().toStdString());
    return -1;
  }
  ssize_t ret_val = static_cast<ssize_t>(
      client_socket_->write(buf, static_cast<qint64>(length)));
  client_socket_->waitForBytesWritten();
  client_socket_->flush();
  return ret_val;
}
