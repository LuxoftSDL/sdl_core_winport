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

#ifdef OS_WINDOWS
#include <stdlib.h>
#include <sys/types.h>
#include <io.h>
#include <BaseTsd.h>
#include <fcntl.h>

#define SDP_RECORD_SIZE   0x0000004f
union {
	CHAR buf[5000];
	SOCKADDR_BTH _Unused_;
} butuh;

struct{
	BTHNS_SETBLOB   b;
	unsigned char   uca[SDP_RECORD_SIZE];
} bigBlob;
#else
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <vector>
#include <sstream>
#include "transport_manager/bluetooth/bluetooth_transport_adapter.h"
#include "transport_manager/bluetooth/bluetooth_device.h"

#include "utils/logger.h"
#include "utils/threads/thread.h"

namespace transport_manager {
namespace transport_adapter {

CREATE_LOGGERPTR_GLOBAL(logger_, "TransportManager")

namespace {
char* SplitToAddr(char* dev_list_entry) {
  char* bl_address = strtok(dev_list_entry, "()");
  if (bl_address != NULL) {
    bl_address = strtok(NULL, "()");
    return bl_address;
  } else {
    return NULL;
  }
}

int FindPairedDevs(std::vector<BTH_ADDR*>* result) {
  LOG4CXX_TRACE(logger_, "enter. result adress: " << result);
  DCHECK(result != NULL);

  const char* cmd = "bt-device -l";

  FILE* pipe = _popen(cmd, "r");
  if (!pipe) {
    LOG4CXX_TRACE(logger_, "exit -1. Condition: !pipe");
    return -1;
  }
  char* buffer = new char[1028];
  size_t counter = 0;
  while (fgets(buffer, 1028, pipe) != NULL) {
    if (0 < counter++) {  //  skip first line
      char* bt_address = SplitToAddr(buffer);
      if (bt_address) {
		  BTH_ADDR address;

		std::wstringstream w_address_record;
		w_address_record << bt_address;
		int address_size = sizeof(struct sockaddr_storage);
		int ret_val = WSAStringToAddress((LPSTR)w_address_record.str().c_str(),
											AF_INET,
											NULL,
											(LPSOCKADDR)&address,
											&address_size);
		if (ret_val != 0){
			LOG4CXX_ERROR(logger_, "WSAStringToAddress() failed with error code" << WSAGetLastError());
		}

        result->push_back(&address);
      }
    }
    delete [] buffer;
    buffer = new char[1028];
  }
  _pclose(pipe);
  LOG4CXX_TRACE(logger_, "exit with 0");
  return 0;
}
#ifdef OS_WINDOWS
int __stdcall callback(ULONG uAttribId, LPBYTE pValueStream, ULONG cbStreamSize, LPVOID pvParam){
	SDP_ELEMENT_DATA element;
	if (BluetoothSdpGetElementData(pValueStream, cbStreamSize, &element) != ERROR_SUCCESS){

		if (element.specificType == SDP_ST_UINT8){
			return element.data.uint8;
		}
		else if (element.specificType == SDP_ST_INT8){
			return element.data.int8;
		}
		else if (element.specificType == SDP_ST_UINT16){
			return element.data.uint16;
		}
		else if (element.specificType == SDP_ST_INT16){
			return element.data.int16;
		}
		else if (element.specificType == SDP_ST_UINT32){
			return element.data.uint32;
		}
		else if (element.specificType == SDP_ST_INT32){
			return element.data.int32;
		}
	}
	return 0;
}

#endif
}  //  namespace

BluetoothDeviceScanner::BluetoothDeviceScanner(
  TransportAdapterController* controller, bool auto_repeat_search,
  int auto_repeat_pause_sec)
  : controller_(controller),
	wsaStartup_(2, 2),
    thread_(NULL),
    shutdown_requested_(false),
    ready_(true),
    device_scan_requested_(false),
    device_scan_requested_lock_(),
    device_scan_requested_cv_(),
    auto_repeat_search_(auto_repeat_search),
    auto_repeat_pause_sec_(auto_repeat_pause_sec) {
	BYTE smart_device_link_service_uuid_data[] = { 0x93, 0x6D, 0xA0, 0x1F,
                                                    0x9A, 0xBD, 0x4D, 0x9D, 0x80, 0xC7, 0x02, 0xAF, 0x85, 0xC8, 0x22, 0xA8
                                                  };
	ULONG recordHandle = 0;
	ULONG ulSdpVersion = BTH_SDP_VERSION;
	BLOB blob;
	bigBlob.b.pRecordHandle = (HANDLE *)&smart_device_link_service_uuid_data;
	bigBlob.b.pSdpVersion = &ulSdpVersion;
	bigBlob.b.ulRecordLength = SDP_RECORD_SIZE;
	memcpy(bigBlob.b.pRecord, smart_device_link_service_uuid_data, SDP_RECORD_SIZE);

	blob.cbSize = sizeof(BTHNS_SETBLOB) + SDP_RECORD_SIZE - 1;
	blob.pBlobData = (PBYTE)&bigBlob;

	memset(&smart_device_link_service_uuid_, 0, sizeof(smart_device_link_service_uuid_));
	smart_device_link_service_uuid_.dwSize = sizeof(smart_device_link_service_uuid_);
	smart_device_link_service_uuid_.lpBlob = &blob;
	smart_device_link_service_uuid_.dwNameSpace = NS_BTH;

	if (WSASetService(&smart_device_link_service_uuid_, RNRSERVICE_REGISTER, 0) == SOCKET_ERROR)
	{
		LOG4CXX_ERROR(logger_,"WSASetService() failed with error code " << WSAGetLastError());
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
  LPWSAQUERYSET pwsaResults;
  DWORD dwSize;
  BTH_ADDR btAddr;

  pwsaResults = (LPWSAQUERYSET)butuh.buf;
  dwSize = sizeof(butuh.buf);

  ZeroMemory(&smart_device_link_service_uuid_, sizeof(smart_device_link_service_uuid_));
  smart_device_link_service_uuid_.dwSize = sizeof(smart_device_link_service_uuid_);
  smart_device_link_service_uuid_.dwNameSpace = NS_BTH;
  smart_device_link_service_uuid_.lpcsaBuffer = NULL;
  if (WSALookupServiceBegin(&smart_device_link_service_uuid_, 0, &device_handle) != 0) {
    LOG4CXX_INFO(logger_, "HCI device is not available");
    shutdown_requested_ = true;
    controller_->SearchDeviceFailed(SearchDeviceError());
    return;
  }
  ZeroMemory(pwsaResults, sizeof(WSAQUERYSET));
  pwsaResults->dwSize = sizeof(WSAQUERYSET);
  pwsaResults->dwNameSpace = NS_BTH;
  pwsaResults->lpBlob = NULL;

  if (paired_devices_.empty()) {
    LOG4CXX_INFO(logger_, "Searching for paired devices.");
    if (-1 == FindPairedDevs(&paired_devices_)) {
      LOG4CXX_ERROR(logger_, "Failed to retrieve list of paired devices.");
      controller_->SearchDeviceFailed(SearchDeviceError());
    }
  }

  LOG4CXX_INFO(logger_, "Check rfcomm channel on "
               << paired_devices_.size() << " paired devices.");

  paired_devices_with_sdl_.clear();
  CheckSDLServiceOnDevices(paired_devices_, (int)device_handle,
                           &paired_devices_with_sdl_);
  UpdateTotalDeviceList();

  LOG4CXX_INFO(logger_, "Starting hci_inquiry on device " << device_id);
  std::vector < BTH_ADDR *> found_devices;
  int number_of_devices = -1;
  while (WSALookupServiceNext(device_handle, LUP_RETURN_NAME | LUP_RETURN_ADDR, &dwSize, pwsaResults) == 0){
	  ++number_of_devices;
	  found_devices[number_of_devices] = ((BTH_ADDR*)pwsaResults->lpcsaBuffer->RemoteAddr.lpSockaddr);
  }
    found_devices_with_sdl_.clear();
    CheckSDLServiceOnDevices(found_devices, (int)device_handle,
                             &found_devices_with_sdl_);

  UpdateTotalDeviceList();
  controller_->FindNewApplicationsRequest();

  WSALookupServiceEnd(device_handle);

  if (number_of_devices < 0) {
    LOG4CXX_DEBUG(logger_, "number_of_devices < 0");
    controller_->SearchDeviceFailed(SearchDeviceError());
  }
}

void BluetoothDeviceScanner::CheckSDLServiceOnDevices(
	const std::vector<BTH_ADDR*>& bd_addresses, int device_handle,
  DeviceVector* discovered_devices) {
  LOG4CXX_TRACE(logger_, "enter. bd_addresses: " << &bd_addresses << ", device_handle: " <<
                device_handle << ", discovered_devices: " << discovered_devices);
  std::vector<RfcommChannelVector> sdl_rfcomm_channels =
    DiscoverSmartDeviceLinkRFCOMMChannels(bd_addresses);

  for (size_t i = 0; i < bd_addresses.size(); ++i) {
    if (sdl_rfcomm_channels[i].empty()) {
      continue;
    }

	const BTH_ADDR& bd_address = *bd_addresses[i];
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
const std::vector<BTH_ADDR*>& device_addresses) {
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
                           *device_addresses[i], &result[i]);
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
  LOG4CXX_TRACE(logger_, "exit with vector<RfcommChannelVector>: size = " << result.size());
  return result;
}


bool BluetoothDeviceScanner::DiscoverSmartDeviceLinkRFCOMMChannels(
	const BTH_ADDR& device_address, RfcommChannelVector* channels) {
	LOG4CXX_TRACE(logger_, "enter. device_address: " << &device_address << ", channels: " <<
		channels);
	int iResult = 0;
	int iRet;
	BLOB blob;
	SOCKADDR_BTH socket_addres;
	CSADDR_INFO socket_addres_info;
	HANDLE hLookup1;
	CHAR buf1[5000];
	DWORD dwSize;
	LPWSAQUERYSET pwsaResults1;
	BTHNS_RESTRICTIONBLOB RBlob;

	memset(&RBlob, 0, sizeof(RBlob));
	RBlob.type = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
	RBlob.numRange = 1;
	RBlob.pRange[0].minAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.pRange[0].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.uuids[0].uuidType = SDP_ST_UUID16;
	RBlob.uuids[0].u.uuid16 = SerialPortServiceClassID_UUID16;

	blob.cbSize = sizeof(RBlob);
	blob.pBlobData = (BYTE *)&RBlob;

	memset(&socket_addres, 0, sizeof(socket_addres));
	socket_addres.btAddr = device_address;
	socket_addres.addressFamily = AF_BTH;

	memset(&socket_addres_info, 0, sizeof(socket_addres_info));
	socket_addres_info.RemoteAddr.lpSockaddr = (SOCKADDR *)&socket_addres;
	socket_addres_info.RemoteAddr.iSockaddrLength = sizeof(socket_addres);

	memset(&smart_device_link_service_uuid_, 0, sizeof(smart_device_link_service_uuid_));
	smart_device_link_service_uuid_.dwSize = sizeof(smart_device_link_service_uuid_);
	smart_device_link_service_uuid_.dwNameSpace = NS_ALL;
	smart_device_link_service_uuid_.lpBlob = &blob;
	smart_device_link_service_uuid_.lpcsaBuffer = &socket_addres_info;

	iRet = WSALookupServiceBegin(&smart_device_link_service_uuid_, 0, &hLookup1);
	int proto = 0;
	bool ready_push_to_channels = false;
	if (iRet == 0){
		pwsaResults1 = (LPWSAQUERYSET)buf1;
		dwSize = sizeof(buf1);
		memset(pwsaResults1, 0, sizeof(WSAQUERYSET));
		pwsaResults1->dwSize = sizeof(WSAQUERYSET);
		pwsaResults1->dwNameSpace = NS_BTH;
		pwsaResults1->lpBlob = NULL;
		while (WSALookupServiceNext(hLookup1, 0, &dwSize, pwsaResults1) != 0){
			CSADDR_INFO * pCSAddr = (CSADDR_INFO *)pwsaResults1->lpcsaBuffer;

			if (pwsaResults1->lpBlob)
			{
				BLOB * pBlob = (BLOB*)pwsaResults1->lpBlob;
				if (ready_push_to_channels){
					if (proto == RFCOMM_PROTOCOL_UUID16) {
						uint8_t data = static_cast<uint8_t>(BluetoothSdpEnumAttributes(pBlob->pBlobData, pBlob->cbSize, callback, 0));
						channels->push_back(data);
					}
				}
				else{
					proto = BluetoothSdpEnumAttributes(pBlob->pBlobData, pBlob->cbSize, callback, 0);
				}
			}
		}
		if (WSALookupServiceEnd(hLookup1) != 0){
			LOG4CXX_ERROR(logger_, "BtServiceSearch(): WSALookupServiceEnd(hLookup1) failed with error code ", WSAGetLastError());
		}
	}
	else{
		LOG4CXX_ERROR(logger_, "BtServiceSearch(): WSALookupServiceBegin() failed with error code ", WSAGetLastError());
	}

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

