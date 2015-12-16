/*
 * Copyright (c) 2014-2015, Ford Motor Company
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
#if defined(OS_WINDOWS)
#include "utils/winhdr.h"

#ifdef MSG_NOSIGNAL
#undef MSG_NOSIGNAL
#endif
#define MSG_NOSIGNAL 0

#elif defined(OS_POSIX)
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "utils/logger.h"
#include "utils/socket.h"
#include "media_manager/socket_streamer_adapter.h"

namespace media_manager {

CREATE_LOGGERPTR_GLOBAL(logger, "SocketStreamerAdapter")

SocketStreamerAdapter::SocketStreamerAdapter(
    const std::string& ip,
    int32_t port,
    const std::string& header)
  : StreamerAdapter(new SocketStreamer(this, ip, port, header)) {
}

SocketStreamerAdapter::~SocketStreamerAdapter() {
}

SocketStreamerAdapter::SocketStreamer::SocketStreamer(
    SocketStreamerAdapter* const adapter,
    const std::string& ip,
    int32_t port,
    const std::string& header)
  : Streamer(adapter),
    ip_(ip),
    port_(port),
    header_(header),
    is_first_frame_(true) {
}

SocketStreamerAdapter::SocketStreamer::~SocketStreamer() {
}

bool SocketStreamerAdapter::SocketStreamer::Connect() {
  LOG4CXX_AUTO_TRACE(logger);

  if (!socket_.Create(AF_INET, SOCK_STREAM, 0)) {
    LOG4CXX_ERROR(logger, "Unable to create socket");
    return false;
  }

  int32_t optval = 1;
  if (!socket_.SetOptions(SOL_SOCKET, SO_REUSEADDR,
                          reinterpret_cast<const char*>(&optval),
                          sizeof optval)) {
    LOG4CXX_ERROR(logger, "Unable to set sockopt");
    return false;
  }

  struct sockaddr_in serv_addr = { 0 };
  serv_addr.sin_addr.s_addr = inet_addr(ip_.c_str());
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_);
  if (!socket_.Bind(reinterpret_cast<struct sockaddr*>(&serv_addr),
                    sizeof(serv_addr))) {
    LOG4CXX_ERROR(logger, "Unable to bind");
    return false;
  }

  if (!socket_.Listen(5)) {
    LOG4CXX_ERROR(logger, "Unable to listen");
    return false;
  }

  struct sockaddr_in client_addr = { 0 };
  size_t client_addr_length = sizeof(client_addr);
  send_socket_ = socket_.Accept(
    reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_length);
  if (!send_socket_.Valid()) {
    LOG4CXX_ERROR(logger, "Unable to accept");
    return false;
  }

  is_first_frame_ = true;
  LOG4CXX_INFO(logger, "Client connected");
  return true;
}

void SocketStreamerAdapter::SocketStreamer::Disconnect() {
  LOG4CXX_AUTO_TRACE(logger);
  send_socket_.Close();
  socket_.Close();
}

bool SocketStreamerAdapter::SocketStreamer::Send(
    protocol_handler::RawMessagePtr msg) {
  LOG4CXX_AUTO_TRACE(logger);
  size_t ret;
  if (is_first_frame_) {
    ret = send_socket_.Send(header_.c_str(), header_.size(), MSG_NOSIGNAL);
    if (ret != header_.size()) {
      LOG4CXX_ERROR(logger, "Unable to send data to socket");
      return false;
    }
    is_first_frame_ = false;
  }

  ret = send_socket_.Send(reinterpret_cast<const char*>(msg->data()),
                          msg->data_size(), MSG_NOSIGNAL);
  if (-1 == ret) {
    LOG4CXX_ERROR(logger, "Unable to send data to socket");
    return false;
  }

  if (ret != msg->data_size()) {
    LOG4CXX_WARN(logger, "Couldn't send all the data to socket");
  }

  LOG4CXX_INFO(logger, "Streamer::sent " << msg->data_size());
  return true;
}

}  // namespace media_manager
