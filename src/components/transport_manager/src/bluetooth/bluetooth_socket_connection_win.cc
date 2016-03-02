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
#include "utils/host_address.h"

#include "utils/logger.h"

namespace transport_manager {
namespace transport_adapter {
CREATE_LOGGERPTR_GLOBAL(logger_, "TransportManager")

BluetoothSocketConnection::BluetoothSocketConnection(
    const DeviceUID& device_uid,
    const ApplicationHandle& app_handle,
    TransportAdapterController* controller)
    : ThreadedSocketConnection(device_uid, app_handle, controller) {}

BluetoothSocketConnection::~BluetoothSocketConnection() {}

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

  SOCKET rfcomm_socket;

  int attempts = 4;
  int connect_status = 0;
  LOGGER_DEBUG(logger_, "start rfcomm Connect attempts");
  do {
    rfcomm_socket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (-1 == rfcomm_socket) {
      LOGGER_ERROR_WITH_ERRNO(logger_,
                              "Failed to create RFCOMM socket for device "
                                  << device_handle());
      *error = new ConnectError();
      LOGGER_TRACE(logger_, "exit with FALSE");
      return false;
    }
    const sockaddr* p_sockaddr = reinterpret_cast<const sockaddr*>(&bthAddr);
    connect_status = ::connect(rfcomm_socket, p_sockaddr, sizeof(SOCKADDR_BTH));
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
      closesocket(rfcomm_socket);
    }
    Sleep(2);
  } while (--attempts > 0);
  LOGGER_INFO(logger_, "rfcomm Connect attempts finished");
  if (0 != connect_status) {
    LOGGER_DEBUG(
        logger_,
        "Failed to Connect to remote device "
            << BluetoothDevice::GetUniqueDeviceId(bluetooth_device->address())
            << " for session " << this);
    *error = new ConnectError();
    LOGGER_TRACE(logger_, "exit with FALSE");
    return false;
  }

  utils::TcpSocketConnection bth_socket;
  bth_socket.Connect(static_cast<int>(rfcomm_socket));
  SetSocket(bth_socket);
  LOGGER_TRACE(logger_, "exit with TRUE");
  return true;
}

}  // namespace transport_adapter
}  // namespace transport_manager
