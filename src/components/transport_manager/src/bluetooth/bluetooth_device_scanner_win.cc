/*
 * \file bluetooth_device_scanner_win.cc
 * \brief BluetoothDeviceScanner class header file.
 *
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

#include "transport_manager/bluetooth/bluetooth_device_scanner.h"

#include <stdlib.h>
#include <sys/types.h>
#include <io.h>
#include <BaseTsd.h>
#include <fcntl.h>

#include <vector>
#include <sstream>
#include "transport_manager/bluetooth/bluetooth_transport_adapter.h"
#include "transport_manager/bluetooth/bluetooth_device.h"

#include "utils/logger.h"
#include "utils/threads/thread.h"

namespace{
BOOL __stdcall sdp_value_parser(ULONG uAttribId, LPBYTE pValueStream, ULONG cbStreamSize, LPVOID pvParam) {
  SDP_ELEMENT_DATA element;
  DWORD tmp = BluetoothSdpGetElementData(pValueStream, cbStreamSize, &element);
  if (tmp == ERROR_SUCCESS) {
     if (element.type == SDP_TYPE_SEQUENCE){
        return FALSE;
      }
  }
		return TRUE;
}
}// namespace

namespace transport_manager {
namespace transport_adapter {

CREATE_LOGGERPTR_GLOBAL(logger_, "TransportManager")

BluetoothDeviceScanner::BluetoothDeviceScanner(
  TransportAdapterController* controller, bool auto_repeat_search,
  int auto_repeat_pause_sec)
  : controller_(controller),
   thread_(NULL),
   shutdown_requested_(false),
   ready_(true),
   device_scan_requested_(false),
   device_scan_requested_lock_(),
   device_scan_requested_cv_(),
   auto_repeat_search_(auto_repeat_search),
   auto_repeat_pause_sec_(auto_repeat_pause_sec) {
   BYTE smart_device_link_service_uuid_data[] = { 0x93, 0x6D, 0xA0, 0x1F,
                                                    0x9A, 0xBD, 0x4D, 0x9D,
                                                    0x80, 0xC7, 0x02, 0xAF,
                                                    0x85, 0xC8, 0x22, 0xA8 };
   smart_device_link_service_uuid_.Data1 = 
                      smart_device_link_service_uuid_data[0] << 24 & 0xff000000 |
											smart_device_link_service_uuid_data[1] << 16 & 0x00ff0000 |
											smart_device_link_service_uuid_data[2] << 8 & 0x0000ff00 |
											smart_device_link_service_uuid_data[3] & 0x000000ff;
   smart_device_link_service_uuid_.Data2 = 
                      smart_device_link_service_uuid_data[4] << 8 & 0xff00 |
											smart_device_link_service_uuid_data[5] & 0x00ff;
   smart_device_link_service_uuid_.Data3 = 
                      smart_device_link_service_uuid_data[6] << 8 & 0xff00 |
											smart_device_link_service_uuid_data[7] & 0x00ff;

   for (int i = 0; i < 8; i++) {
      smart_device_link_service_uuid_.Data4[i] = smart_device_link_service_uuid_data[i + 8];
   }

  thread_ = threads::CreateThread("BT Device Scaner",
                                  new  BluetoothDeviceScannerDelegate(this));
}

BluetoothDeviceScanner::~BluetoothDeviceScanner() {
  thread_->join();
  delete thread_->delegate();
  threads::DeleteThread(thread_);
}


bool BluetoothDeviceScanner::IsInitialised() const {
  return thread_ && thread_->is_running();
}

void BluetoothDeviceScanner::UpdateTotalDeviceList() {
  LOG4CXX_AUTO_TRACE(logger_);
  DeviceVector devices;
  devices.insert(devices.end(), paired_devices_with_sdl_.begin(),
                 paired_devices_with_sdl_.end());
  devices.insert(devices.end(), found_devices_with_sdl_.begin(),
                 found_devices_with_sdl_.end());
  controller_->SearchDeviceDone(devices);
}

void BluetoothDeviceScanner::DoInquiry() {
  LOG4CXX_AUTO_TRACE(logger_);
  HANDLE device_handle;
 
  BLUETOOTH_DEVICE_SEARCH_PARAMS bluetooth_seraach_param;
  BLUETOOTH_DEVICE_INFO bluetooth_dev_info;
  HBLUETOOTH_DEVICE_FIND hdbluetooth_dev_find_res;
  ZeroMemory(&bluetooth_seraach_param, sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS));

  // set options for how we want to load our list of BT devices
  bluetooth_seraach_param.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
  bluetooth_seraach_param.fReturnAuthenticated = TRUE;
  bluetooth_seraach_param.fReturnRemembered = TRUE;
  bluetooth_seraach_param.fReturnConnected = TRUE;
  bluetooth_seraach_param.fIssueInquiry = TRUE;
  bluetooth_seraach_param.cTimeoutMultiplier = 4;
  bluetooth_seraach_param.hRadio = NULL;

  bluetooth_dev_info.dwSize = sizeof(bluetooth_dev_info);
  std::vector < BLUETOOTH_DEVICE_INFO > found_devices;
  int number_of_devices = -1;
  hdbluetooth_dev_find_res = BluetoothFindFirstDevice(&bluetooth_seraach_param,
                             &bluetooth_dev_info);
  if (hdbluetooth_dev_find_res) {
     do {
       found_devices.push_back(bluetooth_dev_info);
	  } while (BluetoothFindNextDevice(hdbluetooth_dev_find_res, &bluetooth_dev_info));
	  BluetoothFindDeviceClose(hdbluetooth_dev_find_res);
  }
  found_devices_with_sdl_.clear();
  CheckSDLServiceOnDevices(found_devices, (int)hdbluetooth_dev_find_res,
      &found_devices_with_sdl_);

  UpdateTotalDeviceList();
  controller_->FindNewApplicationsRequest();
  if (found_devices.empty()) {
      LOG4CXX_DEBUG(logger_, "number_of_devices < 0");
      controller_->SearchDeviceFailed(SearchDeviceError());
  }
}

void BluetoothDeviceScanner::CheckSDLServiceOnDevices(
	const std::vector<BLUETOOTH_DEVICE_INFO>& bd_addresses, int device_handle,
  DeviceVector* discovered_devices) {
  LOG4CXX_TRACE(logger_, "enter. bd_addresses: " << &bd_addresses << ", device_handle: " <<
                device_handle << ", discovered_devices: " << discovered_devices);
  std::vector<RfcommChannelVector> sdl_rfcomm_channels =
    DiscoverSmartDeviceLinkRFCOMMChannels(bd_addresses);

  for (size_t i = 0; i < bd_addresses.size(); ++i) {
    if (sdl_rfcomm_channels[i].empty()) {
      continue;
    }

   const BLUETOOTH_DEVICE_INFO bd_address = bd_addresses[i];
   char deviceName[256];
   char servInfo[NI_MAXSERV];
   DWORD hci_read_remote_name_ret = getnameinfo((struct sockaddr *)device_handle, 
												  sizeof(struct sockaddr),
												  deviceName,
												  NI_MAXHOST,
												  servInfo, 
												  NI_MAXSERV, NI_NUMERICSERV);

   if (hci_read_remote_name_ret != 0) {
      LOG4CXX_ERROR_WITH_ERRNO(logger_, "hci_read_remote_name failed");
      strncpy(deviceName,
              BluetoothDevice::GetUniqueDeviceId(bd_address).c_str(),
              sizeof(deviceName) / sizeof(deviceName[0]));
    }

    Device* bluetooth_device = new BluetoothDevice(bd_address, deviceName,
        sdl_rfcomm_channels[i]);
    if (bluetooth_device) {
      LOG4CXX_INFO(logger_, "Bluetooth device created successfully");
      discovered_devices->push_back(bluetooth_device);
    } else {
      LOG4CXX_WARN(logger_, "Can't create bluetooth device " << deviceName);
    }
  }
  LOG4CXX_TRACE(logger_, "exit");
}

std::vector<BluetoothDeviceScanner::RfcommChannelVector>
BluetoothDeviceScanner::DiscoverSmartDeviceLinkRFCOMMChannels(
const std::vector<BLUETOOTH_DEVICE_INFO>& device_addresses) {
  LOG4CXX_TRACE(logger_, "enter device_addresses: " << &device_addresses);
  const size_t size = device_addresses.size();
  std::vector<RfcommChannelVector> result(size);

  static const int attempts = 4;
  static const int attempt_timeout = 5;
  std::vector<bool> processed(size, false);
  unsigned processed_count = 0;
  for (int nattempt = 0; nattempt < attempts; ++nattempt) {
    for (size_t i = 0; i < size; ++i) {
      if (processed[i]) {
        continue;
      }
      const bool final = DiscoverSmartDeviceLinkRFCOMMChannels(
                           device_addresses[i], &result[i]);
      if (final) {
        processed[i] = true;
        ++processed_count;
      }
    }
    if (++processed_count >= size) {
      break;
    }
    Sleep(attempt_timeout);
  }
  LOG4CXX_TRACE(logger_, "exit with vector<RfcommChannelVector>: size = " << 
                  result.size());
  return result;
}


bool BluetoothDeviceScanner::DiscoverSmartDeviceLinkRFCOMMChannels(
	const BLUETOOTH_DEVICE_INFO& device_address, RfcommChannelVector* channels) {
	LOG4CXX_TRACE(logger_, "enter. device_address: " << &device_address << ", channels: " <<
		channels);
	WSAQUERYSET querySet;
	WSAQUERYSET *pResults;
	BLOB blob;
	DWORD bufferLength1;
	BYTE buffer[2000];
	memset(&querySet, 0, sizeof(querySet));
	querySet.dwSize = sizeof(querySet);
	GUID protocol = RFCOMM_PROTOCOL_UUID;
	querySet.lpServiceClassId = &protocol;
	querySet.dwNameSpace = NS_BTH;
	SOCKADDR_BTH remoteSocketAddress;
	memset(&remoteSocketAddress, 0, sizeof(remoteSocketAddress));
	remoteSocketAddress.addressFamily = AF_BTH;

	BTH_ADDR ba = device_address.Address.ullLong;

	DWORD flags = LUP_FLUSHCACHE | LUP_RETURN_NAME | LUP_RETURN_TYPE |
						LUP_RETURN_ADDR | LUP_RETURN_BLOB | LUP_RETURN_COMMENT | LUP_RES_SERVICE;
	BTH_QUERY_SERVICE queryservice;
	memset(&queryservice, 0, sizeof(BTH_QUERY_SERVICE));
	memcpy(&queryservice.uuids, &smart_device_link_service_uuid_, sizeof(GUID));
	queryservice.uuids[0].uuidType = SDP_ST_UUID128;
	queryservice.type = SDP_SERVICE_SEARCH_REQUEST;

	blob.cbSize = sizeof(BTH_QUERY_SERVICE);
	blob.pBlobData = (BYTE *)&queryservice;
	querySet.dwSize = sizeof(WSAQUERYSET);
	querySet.dwNameSpace = NS_BTH;
	int addr_size = sizeof(struct sockaddr_storage);
	DWORD addrSize = sizeof(struct sockaddr_storage);
	char addressAsString[2000];
	WSAAddressToString((LPSOCKADDR)&ba, addr_size, NULL, (LPSTR)addressAsString, &addrSize);
	querySet.lpszContext = (LPSTR)addressAsString;
	querySet.lpBlob = &blob;

	HANDLE hLookup;
	int result = WSALookupServiceBegin(&querySet, flags, &hLookup);
	if (result == 0) {
		while (result == 0) {
			bufferLength1 = sizeof(buffer);
			pResults = (WSAQUERYSET *)&buffer;
			result = WSALookupServiceNext(hLookup, flags, &bufferLength1, pResults);
			if (result == 0) {
				BLOB *pBlob = (BLOB*)pResults->lpBlob;
				BluetoothSdpEnumAttributes(pBlob->pBlobData, pBlob->cbSize, sdp_value_parser, pBlob);
				// Todo: need fix service discovery.
				// on current moment Loopback is not supported by RFCOMM. 
        //see -> https://msdn.microsoft.com/en-us/library/windows/desktop/aa362914(v=vs.85).aspx
			}
			else{
				WSALookupServiceEnd(hLookup);
			}
		}
	}

  channels->push_back(1); //temporary work around 
  if (!channels->empty()) {
    LOG4CXX_INFO(logger_, "channels not empty");
    std::stringstream rfcomm_channels_string;

    for (RfcommChannelVector::const_iterator it = channels->begin();
         it != channels->end(); ++it) {
      if (it != channels->begin()) {
        rfcomm_channels_string << ", ";
      }
      rfcomm_channels_string << static_cast<uint32_t>(*it);
    }

    LOG4CXX_INFO(logger_,
                 "SmartDeviceLink service was discovered on device "
                 << BluetoothDevice::GetUniqueDeviceId(device_address)
                 << " at channel(s): " << rfcomm_channels_string.str().c_str());
  } else {
    LOG4CXX_INFO(logger_,
                 "SmartDeviceLink service was not discovered on device "
                 << BluetoothDevice::GetUniqueDeviceId(device_address));
  }
  LOG4CXX_TRACE(logger_, "exit with TRUE");
  return true;
}

void BluetoothDeviceScanner::Thread() {
  LOG4CXX_AUTO_TRACE(logger_);
  ready_ = true;
  if (auto_repeat_search_) {
    while (!shutdown_requested_) {
      DoInquiry();
      device_scan_requested_ = false;
      TimedWaitForDeviceScanRequest();
    }
  } else {  // search only on demand
    while (true) {
      {
        sync_primitives::AutoLock auto_lock(device_scan_requested_lock_);
        while (!(device_scan_requested_ || shutdown_requested_)) {
          device_scan_requested_cv_.Wait(auto_lock);
        }
      }
      if (shutdown_requested_) {
        break;
      }
      DoInquiry();
      device_scan_requested_ = false;
    }
  }
}

void BluetoothDeviceScanner::TimedWaitForDeviceScanRequest() {
  LOG4CXX_AUTO_TRACE(logger_);

  if (auto_repeat_pause_sec_ == 0) {
    LOG4CXX_TRACE(logger_, "exit. Condition: auto_repeat_pause_sec_ == 0");
    return;
  }

  {
    sync_primitives::AutoLock auto_lock(device_scan_requested_lock_);
    while (!(device_scan_requested_ || shutdown_requested_)) {
      const sync_primitives::ConditionalVariable::WaitStatus wait_status =
        device_scan_requested_cv_.WaitFor(auto_lock, auto_repeat_pause_sec_ * 1000);
      if (wait_status == sync_primitives::ConditionalVariable::kTimeout) {
        LOG4CXX_INFO(logger_, "Bluetooth scanner timeout, performing scan");
        device_scan_requested_ = true;
      }
    }
  }
}

TransportAdapter::Error BluetoothDeviceScanner::Init() {
  LOG4CXX_AUTO_TRACE(logger_);
  if(!thread_->start()) {
    LOG4CXX_ERROR(logger_, "Bluetooth device scanner thread start failed");
    return TransportAdapter::FAIL;
  }
  LOG4CXX_INFO(logger_, "Bluetooth device scanner thread started");
  return TransportAdapter::OK;
}

void BluetoothDeviceScanner::Terminate() {
  LOG4CXX_AUTO_TRACE(logger_);
  shutdown_requested_ = true;
  if (thread_) {
    {
      sync_primitives::AutoLock auto_lock(device_scan_requested_lock_);
      device_scan_requested_ = false;
      device_scan_requested_cv_.NotifyOne();
    }
    LOG4CXX_INFO(logger_,
                 "Waiting for bluetooth device scanner thread termination");
    thread_->stop();
    LOG4CXX_INFO(logger_, "Bluetooth device scanner thread stopped");
  }
}

TransportAdapter::Error BluetoothDeviceScanner::Scan() {
  LOG4CXX_AUTO_TRACE(logger_);
  if ((!IsInitialised()) || shutdown_requested_) {
    LOG4CXX_WARN(logger_, "BAD_STATE");
    return TransportAdapter::BAD_STATE;
  }
  if (auto_repeat_pause_sec_ == 0) {
    return TransportAdapter::OK;
  }
  TransportAdapter::Error ret = TransportAdapter::OK;

  sync_primitives::AutoLock auto_lock(device_scan_requested_lock_);
  if (!device_scan_requested_) {
    LOG4CXX_TRACE(logger_, "Requesting device Scan");
    device_scan_requested_ = true;
    device_scan_requested_cv_.NotifyOne();
  } else {
    ret = TransportAdapter::BAD_STATE;
    LOG4CXX_WARN(logger_, "BAD_STATE");
  }
  return ret;
}

BluetoothDeviceScanner::BluetoothDeviceScannerDelegate::BluetoothDeviceScannerDelegate(
    BluetoothDeviceScanner* scanner)
  : scanner_(scanner) {
}

void BluetoothDeviceScanner::BluetoothDeviceScannerDelegate::threadMain() {
  LOG4CXX_AUTO_TRACE(logger_);
  DCHECK(scanner_);
  scanner_->Thread();
}

}  // namespace transport_adapter
}  // namespace transport_manager

