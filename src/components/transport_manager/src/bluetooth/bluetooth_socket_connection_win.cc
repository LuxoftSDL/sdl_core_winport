/*
 *
 * Copyright (c) 2013, Ford Motor Company
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

#include "transport_manager/bluetooth/bluetooth_socket_connection.h"

#include <io.h>
#include "transport_manager/bluetooth/bluetooth_device.h"
#include "transport_manager/transport_adapter/transport_adapter_controller.h"
#include "utils/threads/thread.h"

#include "utils/logger.h"

CREATE_LOGGERPTR_GLOBAL(logger_, "TransportManager")
namespace {

bool CloseSocket(SOCKET& socket) {
  LOGGER_AUTO_TRACE(logger_);
  LOGGER_DEBUG(logger_, "Closing socket " << socket);
  if (NULL == socket) {
    LOGGER_DEBUG(logger_,
                 "Socket " << socket << " is not valid. Skip closing.");
    return true;
  }
  if (SOCKET_ERROR == closesocket(socket)) {
    LOGGER_WARN(logger_,
                "Failed to close socket " << socket << ": "
                                          << WSAGetLastError());
    return false;
  }
  socket = NULL;
  return true;
}

HANDLE CreateNotifyEvent() {
  LOGGER_AUTO_TRACE(logger_);
  HANDLE result = CreateEvent(NULL,   // no security attribute
                              true,   // is manual-reset event
                              false,  // initial state = non-signaled
                              NULL);  // unnamed event object
  DCHECK(result);
  return result;
}

}  // namespace

namespace transport_manager {
namespace transport_adapter {
BluetoothSocketConnection::BluetoothSocketConnection(
    const DeviceUID& device_uid,
    const ApplicationHandle& app_handle,
    TransportAdapterController* controller)
    : controller_(controller)
    , frames_to_send_()
    , frames_to_send_mutex_()
    , rfcomm_socket_()
    , terminate_flag_(false)
    , unexpected_disconnect_(false)
    , device_uid_(device_uid)
    , app_handle_(app_handle)
    , notify_event_(NULL) {
  const std::string thread_name = std::string("Socket ") + device_handle();
  thread_ = threads::CreateThread(thread_name.c_str(),
                                  new BthConnectionDelegate(this));
}

BluetoothSocketConnection::~BluetoothSocketConnection() {
  LOGGER_AUTO_TRACE(logger_);
  Disconnect();
  thread_->join();
  delete thread_->delegate();
  threads::DeleteThread(thread_);
}

TransportAdapter::Error BluetoothSocketConnection::Start() {
  LOGGER_AUTO_TRACE(logger_);
  if (!thread_->start()) {
    LOGGER_ERROR(logger_, "thread creation failed");
    return TransportAdapter::FAIL;
  }
  LOGGER_INFO(logger_, "thread created");
  return TransportAdapter::OK;
}

void BluetoothSocketConnection::threadMain() {
  LOGGER_AUTO_TRACE(logger_);
  controller_->ConnectionCreated(this, device_uid_, app_handle_);
  ConnectError* connect_error = NULL;
  if (!Establish(&connect_error)) {
    LOGGER_ERROR(logger_, "Connection Establish failed");
    delete connect_error;
  }
  LOGGER_DEBUG(logger_, "Connection established");
  controller_->ConnectDone(device_handle(), application_handle());
  while (!terminate_flag_) {
    Transmit();
  }
  LOGGER_DEBUG(logger_, "Connection is to finalize");
  Finalize();
  sync_primitives::AutoLock auto_lock(frames_to_send_mutex_);
  while (!frames_to_send_.empty()) {
    LOGGER_INFO(logger_, "removing message");
    ::protocol_handler::RawMessagePtr message = frames_to_send_.front();
    frames_to_send_.pop();
    controller_->DataSendFailed(
        device_handle(), application_handle(), message, DataSendError());
  }
}

void BluetoothSocketConnection::Transmit() {
  LOGGER_AUTO_TRACE(logger_);
  LOGGER_DEBUG(logger_, "Waiting for connection events. " << this);

  if (!IsValid()) {
    LOGGER_ERROR(logger_, "Cannot wait. Not connected.");
    Close();
    return;
  }

  WSANETWORKEVENTS net_events;
  HANDLE socketEvent = WSACreateEvent();
  WSAEventSelect(rfcomm_socket_, socketEvent, FD_WRITE | FD_READ | FD_CLOSE);

  const int events_to_wait_count = 2;
  HANDLE events_to_wait[events_to_wait_count] = {socketEvent, notify_event_};

  DWORD waited_index = WSAWaitForMultipleEvents(
      events_to_wait_count, events_to_wait, false, WSA_INFINITE, false);

  // We've waited for events. We should reset the
  // notify_event_. So on the next enter to the Wait it will
  // be blocked
  ResetEvent(notify_event_);

  const bool is_socket_event = (waited_index == WAIT_OBJECT_0);
  const bool is_notify_event = (waited_index == WAIT_OBJECT_0 + 1);

  if (is_socket_event || is_notify_event) {
    LOGGER_DEBUG(logger_,
                 "Waited event for the connection "
                     << rfcomm_socket_ << ". Socket event: " << is_socket_event
                     << ". Notify event: " << is_notify_event);
  } else {
    LOGGER_ERROR(logger_,
                 "Wait for socket or notification has failed with error: "
                     << WSAGetLastError());
    OnError(WSAGetLastError());
    return;
  }
  bool is_can_write = false;
  if (is_socket_event) {
    if (WSAEnumNetworkEvents(rfcomm_socket_,
                             events_to_wait[waited_index - WAIT_OBJECT_0],
                             &net_events) == SOCKET_ERROR) {
      LOGGER_ERROR(logger_,
                   "Failed to enum socket events: " << WSAGetLastError());
      OnError(WSAGetLastError());
      return;
    }
    if (net_events.lNetworkEvents & FD_READ) {
      LOGGER_DEBUG(logger_, "Network event: FD_READ");
      OnRead();
      return;
    }
    if (net_events.lNetworkEvents & FD_WRITE) {
      LOGGER_DEBUG(logger_, "Network event: FD_WRITE");
      is_can_write = true;
    }
    if (net_events.lNetworkEvents & FD_CLOSE) {
      LOGGER_DEBUG(logger_,
                   "Network event: FD_CLOSE. "
                       << "Connection " << this << " terminated");
      OnClose();
      return;
    }
  }

  // Means that we received notify, thus
  // caller have something to send.
  // Or we received FD_WRITE(can write)
  // event from the socket
  if (is_notify_event || is_can_write) {
    OnWrite();
  }
  LOGGER_DEBUG(logger_, "Waited for connection events: " << this);
}

void BluetoothSocketConnection::Send() {
  LOGGER_AUTO_TRACE(logger_);
  LOGGER_DEBUG(logger_, "Trying to send data if available");
  FrameQueue frames_to_send;
  {
    sync_primitives::AutoLock auto_lock(frames_to_send_mutex_);
    std::swap(frames_to_send, frames_to_send_);
  }

  size_t offset = 0;
  while (!frames_to_send.empty()) {
    ::protocol_handler::RawMessagePtr frame = frames_to_send.front();
    std::size_t bytes_sent = 0u;
    const bool sent =
        Send(reinterpret_cast<const char*>(frame->data() + offset),
             frame->data_size() - offset,
             bytes_sent);
    if (!sent) {
      LOGGER_ERROR(logger_, "Send failed for connection " << this);
      frames_to_send.pop();
      offset = 0;
      controller_->DataSendFailed(
          device_handle(), application_handle(), frame, DataSendError());
      Abort();
      return;
    }
    if (bytes_sent >= 0) {
      offset += bytes_sent;
      if (offset == frame->data_size()) {
        frames_to_send.pop();
        offset = 0;
        controller_->DataSendDone(device_handle(), application_handle(), frame);
      }
    }
  }
}

bool BluetoothSocketConnection::Send(const char* const buffer,
                                     std::size_t size,
                                     std::size_t& bytes_written) {
  bytes_written = 0u;
  if (!IsValid()) {
    LOGGER_ERROR(logger_, "Failed to send data socket is not valid");
    return false;
  }
  const int flags = 0;
  int written = send(rfcomm_socket_, buffer, size, flags);
  int socket_error = WSAGetLastError();
  if (SOCKET_ERROR == written) {
    if (WSAEWOULDBLOCK != socket_error) {
      LOGGER_ERROR(logger_, "Failed to send data: " << socket_error);
      return false;
    } else {
      return true;
    }
  }
  // Lets double chek written because we have signed to unsigned conversion
  DCHECK(written >= 0);
  bytes_written = static_cast<size_t>(written);
  LOGGER_DEBUG(logger_,
               "Sent " << written << " bytes to socket " << rfcomm_socket_);
  return true;
}

bool BluetoothSocketConnection::Close() {
  if (!IsValid()) {
    LOGGER_DEBUG(logger_, "Connection is not valid. Nothing to close.");
    return true;
  }
  // Possibly we're waiting on Wait. We have to interrupt this.
  Notify();

  const BOOL event_closed = CloseHandle(notify_event_);
  if (!event_closed) {
    LOGGER_WARN(logger_, "Failed to close event handler");
  }

  const bool socket_closed = CloseSocket(rfcomm_socket_);
  if (!socket_closed) {
    LOGGER_WARN(logger_, "Failed to close socket handler");
  }

  return event_closed && socket_closed;
}

bool BluetoothSocketConnection::IsValid() const {
  return rfcomm_socket_ != NULL;
}

void BluetoothSocketConnection::OnError(int error) {
  LOGGER_ERROR(logger_, "Connection error: " << error);
  Abort();
}

void BluetoothSocketConnection::OnData(const uint8_t* const buffer,
                                       std::size_t buffer_size) {
  protocol_handler::RawMessagePtr frame(
      new protocol_handler::RawMessage(0, 0, buffer, buffer_size));
  controller_->DataReceiveDone(device_handle(), application_handle(), frame);
}

void BluetoothSocketConnection::OnCanWrite() {
  LOGGER_DEBUG(logger_, "OnCanWrite event. Trying to send data.");
  Send();
}

bool BluetoothSocketConnection::Establish(ConnectError** error) {
  LOGGER_AUTO_TRACE(logger_);
  LOGGER_DEBUG(logger_, "error: " << error);
  DeviceSptr device = controller()->FindDevice(device_handle());
  BluetoothDevice* bluetooth_device =
      static_cast<BluetoothDevice*>(device.get());
  SOCKADDR_BTH bthAddr = bluetooth_device->getSocketBthAddr();

  if (bthAddr.port == NULL) {
    LOGGER_DEBUG(logger_,
                 "Application " << application_handle() << " not found");
    *error = new ConnectError();
    LOGGER_TRACE(logger_, "exit with FALSE");
    return false;
  }

  int attempts = 4;
  int connect_status = 0;
  LOGGER_DEBUG(logger_, "start rfcomm Connect attempts");
  do {
    rfcomm_socket_ = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (-1 == rfcomm_socket_) {
      LOGGER_ERROR_WITH_ERRNO(logger_,
                              "Failed to create RFCOMM socket for device "
                                  << device_handle());
      LOGGER_TRACE(logger_, "exit with FALSE");
      return false;
    }
    const sockaddr* p_sockaddr = reinterpret_cast<const sockaddr*>(&bthAddr);
    connect_status = connect(rfcomm_socket_, p_sockaddr, sizeof(SOCKADDR_BTH));
    if (0 == connect_status) {
      LOGGER_DEBUG(logger_, "rfcomm Connect ok");
      break;
    }
    if (errno != 111 && errno != 104) {
      LOGGER_DEBUG(logger_, "rfcomm Connect errno " << errno);
      break;
    }
    if (errno) {
      LOGGER_DEBUG(logger_, "rfcomm Connect errno " << errno);
      CloseSocket(rfcomm_socket_);
    }
    Sleep(2);
  } while (--attempts > 0);
  LOGGER_INFO(logger_, "rfcomm Connect attempts finished");
  if (0 != connect_status) {
    LOGGER_TRACE(logger_, "exit with FALSE");
    LOGGER_DEBUG(
        logger_,
        "Failed to Connect to remote device "
            << BluetoothDevice::GetUniqueDeviceId(bluetooth_device->address())
            << " for session " << this);
    *error = new ConnectError();
    return false;
  }
  notify_event_ = CreateNotifyEvent();
  LOGGER_TRACE(logger_, "exit with TRUE");
  return true;
}

TransportAdapter::Error BluetoothSocketConnection::Notify() {
  return SetEvent(notify_event_) ? TransportAdapter::OK
                                 : TransportAdapter::FAIL;
}

void BluetoothSocketConnection::Abort() {
  LOGGER_AUTO_TRACE(logger_);
  unexpected_disconnect_ = true;
  terminate_flag_ = true;
}

void BluetoothSocketConnection::Finalize() {
  LOGGER_AUTO_TRACE(logger_);
  if (unexpected_disconnect_) {
    LOGGER_DEBUG(logger_, "unexpected_disconnect");
    controller_->ConnectionAborted(
        device_handle(), application_handle(), CommunicationError());
  } else {
    LOGGER_DEBUG(logger_, "not unexpected_disconnect");
    controller_->ConnectionFinished(device_handle(), application_handle());
  }
  CloseSocket(rfcomm_socket_);
}

TransportAdapter::Error BluetoothSocketConnection::SendData(
    ::protocol_handler::RawMessagePtr message) {
  LOGGER_AUTO_TRACE(logger_);
  sync_primitives::AutoLock auto_lock(frames_to_send_mutex_);
  frames_to_send_.push(message);
  return Notify();
}

TransportAdapter::Error BluetoothSocketConnection::Disconnect() {
  LOGGER_AUTO_TRACE(logger_);
  terminate_flag_ = true;
  return Notify();
}

TransportAdapterController* BluetoothSocketConnection::controller() {
  return controller_;
}

DeviceUID BluetoothSocketConnection::device_handle() const {
  return device_uid_;
}

ApplicationHandle BluetoothSocketConnection::application_handle() const {
  return app_handle_;
}

void BluetoothSocketConnection::OnRead() {
  LOGGER_AUTO_TRACE(logger_);
  const std::size_t buffer_size = 4096u;
  char buffer[buffer_size];
  int bytes_read = -1;

  do {
    bytes_read = recv(rfcomm_socket_, buffer, sizeof(buffer), 0);
    if (bytes_read > 0) {
      LOGGER_DEBUG(logger_,
                   "Received " << bytes_read << " bytes from socket "
                               << rfcomm_socket_);
      uint8_t* casted_buffer = reinterpret_cast<uint8_t*>(buffer);
      OnData(casted_buffer, bytes_read);
    } else if (bytes_read < 0) {
      int socket_error = WSAGetLastError();
      if (bytes_read == SOCKET_ERROR && WSAEWOULDBLOCK != socket_error) {
        LOGGER_ERROR(logger_,
                     "recv() failed for connection "
                         << rfcomm_socket_ << ". Error: " << socket_error);
        OnError(socket_error);
        return;
      }
    } else {
      LOGGER_WARN(logger_,
                  "Socket " << rfcomm_socket_ << " closed by remote peer");
      OnError(WSAGetLastError());
      return;
    }
  } while (bytes_read > 0);
}

void BluetoothSocketConnection::OnWrite() {
  OnCanWrite();
}

void BluetoothSocketConnection::OnClose() {
  Abort();
}

////////////////////////////////////////////////////////////////////////////////
/// BthConnectionDelegate::BthConnectionDelegate
////////////////////////////////////////////////////////////////////////////////

BluetoothSocketConnection::BthConnectionDelegate::BthConnectionDelegate(
    BluetoothSocketConnection* connection)
    : connection_(connection) {}

void BluetoothSocketConnection::BthConnectionDelegate::threadMain() {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK(connection_);
  connection_->threadMain();
}

void BluetoothSocketConnection::BthConnectionDelegate::exitThreadMain() {
  LOGGER_AUTO_TRACE(logger_);
}

}  // namespace transport_adapter
}  // namespace transport_manager
