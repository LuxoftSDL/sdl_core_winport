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

#include "transport_manager/bluetooth/bluetooth_device.h"

#ifndef WIN_NATIVE
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>
#else
#ifdef max
#undef max
#endif
#endif

#include <errno.h>
#include <sys/types.h>

#ifndef WIN_NATIVE
#include <sys/socket.h>
#endif

#include <algorithm>
#include <limits>
#include "utils/logger.h"

namespace transport_manager {
namespace transport_adapter {
CREATE_LOGGERPTR_GLOBAL(logger_, "TransportManager")

bool BluetoothDevice::GetRfcommChannel(const ApplicationHandle app_handle,
                                       uint8_t* channel_out) {
  LOG4CXX_TRACE(logger_, "enter. app_handle: " << app_handle << ", channel_out: " <<
                channel_out);
  if (app_handle < 0 || app_handle > std::numeric_limits<uint8_t>::max()) {
    LOG4CXX_TRACE(logger_,
                  "exit with FALSE. Condition: app_handle < 0 || app_handle > numeric_limits::max()");
    return false;
  }
  const uint8_t channel = static_cast<uint8_t>(app_handle);
  RfcommChannelVector::const_iterator it = std::find(rfcomm_channels_.begin(),
      rfcomm_channels_.end(),
      channel);
  if (it == rfcomm_channels_.end()) {
    LOG4CXX_TRACE(logger_, "exit with FALSE. Condition: channel not found in RfcommChannelVector");
    return false;
  }
  *channel_out = channel;
  LOG4CXX_TRACE(logger_, "exit with TRUE");
  return true;
}

#ifdef WIN_NATIVE
std::string BluetoothDevice::GetUniqueDeviceId(const BTH_ADDR & device_address) {
#else
std::string BluetoothDevice::GetUniqueDeviceId(const bdaddr_t& device_address) {
#endif
  LOG4CXX_TRACE(logger_, "enter. device_adress: " << &device_address);
  char device_address_string[32];
#ifdef WIN_NATIVE
  DWORD addrSize = sizeof(struct sockaddr_storage);
  int addr_size = sizeof(struct sockaddr_storage);
  int ret_val = WSAAddressToString( (LPSOCKADDR)&device_address, 
									 addr_size,
									 NULL,
									 device_address_string,
									 &addrSize); 
  if (ret_val != 0){
	  LOG4CXX_ERROR(logger_, "WSAAddressToString() failed with error code" << WSAGetLastError());
  }
#else
  ba2str(&device_address, device_address_string);
#endif
  LOG4CXX_TRACE(logger_, "exit with BT-" << device_address_string);
  return std::string("BT-") + device_address_string;
}

#ifdef WIN_NATIVE
BluetoothDevice::BluetoothDevice(const BTH_ADDR& device_address, const char* device_name,
	const RfcommChannelVector& rfcomm_channels)
#else
BluetoothDevice::BluetoothDevice(const bdaddr_t& device_address, const char* device_name,
                                 const RfcommChannelVector& rfcomm_channels)
#endif
  : Device(device_name, GetUniqueDeviceId(device_address)),
#ifdef WIN_NATIVE
  wsaStartup_(2, 2),
#endif
    address_(device_address),
    rfcomm_channels_(rfcomm_channels) {
}

bool BluetoothDevice::IsSameAs(const Device* other) const {
  LOG4CXX_TRACE(logger_, "enter. device: " << other);
  bool result = false;

  const BluetoothDevice* other_bluetooth_device =
    dynamic_cast<const BluetoothDevice*>(other);

  if (0 != other_bluetooth_device) {
    if (0
        == memcmp(&address_, &other_bluetooth_device->address_,
#ifdef WIN_NATIVE
		sizeof(BTH_ADDR))) {
#else
                  sizeof(bdaddr_t))) {
#endif
      result = true;
    }
  }
  if (result) {
      LOG4CXX_TRACE(logger_, "exit with TRUE");
  } else {
      LOG4CXX_TRACE(logger_, "exit with FALSE");
  }
  return result;
}

ApplicationList BluetoothDevice::GetApplicationList() const {
  return ApplicationList(rfcomm_channels_.begin(), rfcomm_channels_.end());
}

}  // namespace transport_adapter
}  // namespace transport_manager
