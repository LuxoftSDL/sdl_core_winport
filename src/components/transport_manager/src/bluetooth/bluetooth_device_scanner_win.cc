/*
 * \file bluetooth_device_scanner_win.cc
 * \brief BluetoothDeviceScanner class header file.
 *
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
#include "utils/bluetooth_win/bluetooth_service_record.h"
#include "utils/bluetooth_win/bluetooth_uuid.h"

#include "utils/logger.h"
#include "utils/threads/thread.h"

namespace transport_manager {
namespace transport_adapter {

CREATE_LOGGERPTR_GLOBAL(logger_, "TransportManager")

BluetoothDeviceScanner::BluetoothDeviceScanner(
    TransportAdapterController* controller,
    bool auto_repeat_search,
    int auto_repeat_pause_sec)
    : controller_(controller)
    , thread_(NULL)
    , shutdown_requested_(false)
    , ready_(true)
    , device_scan_requested_(false)
    , device_scan_requested_lock_()
    , device_scan_requested_cv_()
    , auto_repeat_search_(auto_repeat_search)
    , auto_repeat_pause_sec_(auto_repeat_pause_sec) {
  BYTE smart_device_link_service_uuid_data[] = {0x93,
                                                0x6D,
                                                0xA0,
                                                0x1F,
                                                0x9A,
                                                0xBD,
                                                0x4D,
                                                0x9D,
                                                0x80,
                                                0xC7,
                                                0x02,
                                                0xAF,
                                                0x85,
                                                0xC8,
                                                0x22,
                                                0xA8};
  utils::ConvertBytesToUUID(smart_device_link_service_uuid_data,
                            &smart_device_link_service_uuid_);
  service_uuid_str_ =
      utils::BluetoothUUID(utils::GuidToStr(smart_device_link_service_uuid_))
          .Value();
  thread_ = threads::CreateThread("BT Device Scaner",
                                  new BluetoothDeviceScannerDelegate(this));
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
  LOGGER_AUTO_TRACE(logger_);
  DeviceVector devices;
  devices.insert(devices.end(),
                 paired_devices_with_sdl_.begin(),
                 paired_devices_with_sdl_.end());
  devices.insert(devices.end(),
                 found_devices_with_sdl_.begin(),
                 found_devices_with_sdl_.end());
  controller_->SearchDeviceDone(devices);
}

void BluetoothDeviceScanner::DoInquiry() {
  LOGGER_AUTO_TRACE(logger_);
  HANDLE radio_handle;
  BLUETOOTH_FIND_RADIO_PARAMS bluetooth_seraach_param = {
      sizeof(bluetooth_seraach_param)};
  HBLUETOOTH_RADIO_FIND hdbluetooth_dev_find_res =
      BluetoothFindFirstRadio(&bluetooth_seraach_param, &radio_handle);

  std::vector<BLUETOOTH_DEVICE_INFO> found_devices;
  int number_of_devices = -1;
  BLUETOOTH_DEVICE_INFO_STRUCT device_info;
  device_info.dwSize = sizeof(device_info);
  BLUETOOTH_DEVICE_SEARCH_PARAMS device_search_params;
  memset(&device_search_params, 0, sizeof(device_search_params));
  device_search_params.dwSize = sizeof(device_search_params);

  device_search_params.fReturnAuthenticated = true;
  device_search_params.fReturnRemembered = true;
  device_search_params.fReturnConnected = true;
  device_search_params.fIssueInquiry = false;
  device_search_params.hRadio = radio_handle;

  if (hdbluetooth_dev_find_res) {
    do {
      BLUETOOTH_RADIO_INFO radio_info;
      radio_info.dwSize = sizeof(radio_info);
      if (ERROR_SUCCESS != BluetoothGetRadioInfo(radio_handle, &radio_info)) {
        LOGGER_WARN(logger_, "Not find bluetooth device");
        return;
      }
      HANDLE device_find =
          BluetoothFindFirstDevice(&device_search_params, &device_info);
      DWORD numServices = sizeof(smart_device_link_service_uuid_);
      if (device_find) {
        do {
          BluetoothEnumerateInstalledServices(radio_handle,
                                              &device_info,
                                              &numServices,
                                              &smart_device_link_service_uuid_);
          if (numServices) {
            found_devices.push_back(device_info);
          }
        } while (BluetoothFindNextDevice(device_find, &device_info));

        BluetoothFindDeviceClose(device_find);
      }
    } while (BluetoothFindNextRadio(hdbluetooth_dev_find_res, &radio_handle));
    CloseHandle(radio_handle);
    BluetoothFindRadioClose(hdbluetooth_dev_find_res);
  }
  found_devices_with_sdl_.clear();
  QueryBthProtocolInfo();
  CheckSDLServiceOnDevices(
      found_devices, (int)hdbluetooth_dev_find_res, &found_devices_with_sdl_);

  UpdateTotalDeviceList();
  controller_->FindNewApplicationsRequest();
  if (found_devices.empty()) {
    LOGGER_DEBUG(logger_, "number_of_devices < 0");
    controller_->SearchDeviceFailed(SearchDeviceError());
  }
}

void BluetoothDeviceScanner::QueryBthProtocolInfo() {
  SOCKET bth_socket = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  DWORD lastError = ::GetLastError();
  if (bth_socket == INVALID_SOCKET) {
    LOGGER_ERROR(logger_,
                 "Failed to get bluetooth socket! "
                     << utils::GetLastErrorMessage(lastError));
    return;
  }
  int protocol_info_size = sizeof(protocol_info_);
  int retVal = ::getsockopt(bth_socket,
                            SOL_SOCKET,
                            SO_PROTOCOL_INFO,
                            (char*)&protocol_info_,
                            &protocol_info_size);
  lastError = ::GetLastError();
  closesocket(bth_socket);
  if (retVal) {
    LOGGER_ERROR(logger_,
                 "Failed to get bluetooth socket options! "
                     << utils::GetLastErrorMessage(lastError));
  }
}

void BluetoothDeviceScanner::CheckSDLServiceOnDevices(
    const std::vector<BLUETOOTH_DEVICE_INFO>& bd_addresses,
    int device_handle,
    DeviceVector* discovered_devices) {
  LOGGER_TRACE(logger_,
               "enter. bd_addresses: " << &bd_addresses << ", device_handle: "
                                       << device_handle
                                       << ", discovered_devices: "
                                       << discovered_devices);
  std::vector<RfcommChannelVector> sdl_rfcomm_channels =
      DiscoverSmartDeviceLinkRFCOMMChannels(bd_addresses);

  for (size_t i = 0; i < bd_addresses.size(); ++i) {
    if (sdl_rfcomm_channels[i].empty()) {
      continue;
    }

    const BLUETOOTH_DEVICE_INFO bd_address = bd_addresses[i];
    char deviceName[256];
    char servInfo[NI_MAXSERV];
    DWORD hci_read_remote_name_ret =
        getnameinfo((struct sockaddr*)device_handle,
                    sizeof(struct sockaddr),
                    deviceName,
                    NI_MAXHOST,
                    servInfo,
                    NI_MAXSERV,
                    NI_NUMERICSERV);

    if (hci_read_remote_name_ret != 0) {
      LOGGER_ERROR_WITH_ERRNO(logger_, "hci_read_remote_name failed");
      strncpy(deviceName,
              BluetoothDevice::GetUniqueDeviceId(bd_address).c_str(),
              sizeof(deviceName) / sizeof(deviceName[0]));
    }

    Device* bluetooth_device = new BluetoothDevice(
        bd_address, deviceName, sdl_rfcomm_channels[i], sock_addr_bth_server_);

    if (bluetooth_device) {
      LOGGER_INFO(logger_, "Bluetooth device created successfully");
      discovered_devices->push_back(bluetooth_device);
    } else {
      LOGGER_WARN(logger_, "Can't create bluetooth device " << deviceName);
    }
  }
  LOGGER_TRACE(logger_, "exit");
}

std::vector<BluetoothDeviceScanner::RfcommChannelVector>
BluetoothDeviceScanner::DiscoverSmartDeviceLinkRFCOMMChannels(
    const std::vector<BLUETOOTH_DEVICE_INFO>& device_addresses) {
  LOGGER_TRACE(logger_, "enter device_addresses: " << &device_addresses);
  const size_t size = device_addresses.size();
  std::vector<RfcommChannelVector> result(size);
  sock_addr_bth_server_ = {0};
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
          device_addresses[i], &result[i], sock_addr_bth_server_);
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
  LOGGER_TRACE(
      logger_,
      "exit with vector<RfcommChannelVector>: size = " << result.size());
  return result;
}

bool BluetoothDeviceScanner::DiscoverSmartDeviceLinkRFCOMMChannels(
    const BLUETOOTH_DEVICE_INFO& device_address,
    RfcommChannelVector* channels,
    SOCKADDR_BTH& sock_addr_bth_server) {
  std::string str_device_address = utils::BthDeviceAddrToStr(device_address);
  HANDLE handle_service_search;
  DWORD flags = LUP_FLUSHCACHE | LUP_RETURN_NAME | LUP_RETURN_TYPE |
                LUP_RETURN_ADDR | LUP_RETURN_BLOB | LUP_RETURN_COMMENT |
                LUP_RES_SERVICE;
  WSAQUERYSET service_search_quiery;
  ZeroMemory(&service_search_quiery, sizeof(service_search_quiery));
  service_search_quiery.dwSize = sizeof(service_search_quiery);
  GUID protocol = L2CAP_PROTOCOL_UUID;
  service_search_quiery.lpServiceClassId = &protocol;
  service_search_quiery.dwNameSpace = NS_BTH;
  service_search_quiery.lpszContext =
      const_cast<LPSTR>(str_device_address.c_str());

  LOGGER_TRACE(logger_, "Start search SPT Service...");
  int service_scan_result = WSALookupServiceBegin(
      &service_search_quiery, LUP_FLUSHCACHE, &handle_service_search);
  if (!service_scan_result) {
    while (true) {
      BYTE buffer[2000];
      DWORD bufferLength = sizeof(buffer);
      WSAQUERYSET* pResults = reinterpret_cast<WSAQUERYSET*>(&buffer);
      if (WSALookupServiceNext(
              handle_service_search, flags, &bufferLength, pResults)) {
        LOGGER_WARN(logger_,
                    "Service scan error:"
                        << utils::GetLastErrorMessage(GetLastError()));
        break;
      } else {
        if (pResults->lpBlob) {
          const BLOB* pBlob = static_cast<BLOB*>(pResults->lpBlob);
          utils::BluetoothServiceRecordWin BtHServiceParser(
              str_device_address,
              std::string(pResults->lpszServiceInstanceName),
              utils::ByteArrayToVector(pBlob),
              utils::BluetoothUUID(service_uuid_str_));
          if (BtHServiceParser.IsUuidEqual(service_uuid_str_)) {
            ULONGLONG ululRemoteBthAddr = 0;
            BTH_ADDR* pRemoteBtAddr =
                static_cast<BTH_ADDR*>(&ululRemoteBthAddr);
            CopyMemory(pRemoteBtAddr,
                       &(reinterpret_cast<PSOCKADDR_BTH>(
                             pResults->lpcsaBuffer->RemoteAddr.lpSockaddr))
                            ->btAddr,
                       sizeof(*pRemoteBtAddr));
            sock_addr_bth_server.addressFamily = AF_BTH;
            sock_addr_bth_server.btAddr = *pRemoteBtAddr;
            sock_addr_bth_server.serviceClassId =
                smart_device_link_service_uuid_;
            sock_addr_bth_server.port = BtHServiceParser.RfcommChannel();
            channels->push_back(BtHServiceParser.RfcommChannel());
          }
        }
      }
    }
    WSALookupServiceEnd(handle_service_search);
  } else {
    LOGGER_ERROR(
        logger_,
        "Service serach error:" << utils::GetLastErrorMessage(GetLastError()));
    return false;
  }
  LOGGER_TRACE(logger_,
               "enter. device_address: " << &str_device_address
                                         << ", channels: "
                                         << channels);
  return true;
}

void BluetoothDeviceScanner::Thread() {
  LOGGER_AUTO_TRACE(logger_);
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
  LOGGER_AUTO_TRACE(logger_);

  if (auto_repeat_pause_sec_ == 0) {
    LOGGER_TRACE(logger_, "exit. Condition: auto_repeat_pause_sec_ == 0");
    return;
  }

  {
    sync_primitives::AutoLock auto_lock(device_scan_requested_lock_);
    while (!(device_scan_requested_ || shutdown_requested_)) {
      const sync_primitives::ConditionalVariable::WaitStatus wait_status =
          device_scan_requested_cv_.WaitFor(auto_lock,
                                            auto_repeat_pause_sec_ * 1000);
      if (wait_status == sync_primitives::ConditionalVariable::kTimeout) {
        LOGGER_INFO(logger_, "Bluetooth scanner timeout, performing scan");
        device_scan_requested_ = true;
      }
    }
  }
}

TransportAdapter::Error BluetoothDeviceScanner::Init() {
  LOGGER_AUTO_TRACE(logger_);
  if (!thread_->start()) {
    LOGGER_ERROR(logger_, "Bluetooth device scanner thread start failed");
    return TransportAdapter::FAIL;
  }
  LOGGER_INFO(logger_, "Bluetooth device scanner thread started");
  return TransportAdapter::OK;
}

void BluetoothDeviceScanner::Terminate() {
  LOGGER_AUTO_TRACE(logger_);
  shutdown_requested_ = true;
  if (thread_) {
    {
      sync_primitives::AutoLock auto_lock(device_scan_requested_lock_);
      device_scan_requested_ = false;
      device_scan_requested_cv_.NotifyOne();
    }
    LOGGER_INFO(logger_,
                "Waiting for bluetooth device scanner thread termination");
    thread_->stop();
    LOGGER_INFO(logger_, "Bluetooth device scanner thread stopped");
  }
}

TransportAdapter::Error BluetoothDeviceScanner::Scan() {
  LOGGER_AUTO_TRACE(logger_);
  if ((!IsInitialised()) || shutdown_requested_) {
    LOGGER_WARN(logger_, "BAD_STATE");
    return TransportAdapter::BAD_STATE;
  }
  if (auto_repeat_pause_sec_ == 0) {
    return TransportAdapter::OK;
  }
  TransportAdapter::Error ret = TransportAdapter::OK;

  sync_primitives::AutoLock auto_lock(device_scan_requested_lock_);
  if (!device_scan_requested_) {
    LOGGER_TRACE(logger_, "Requesting device Scan");
    device_scan_requested_ = true;
    device_scan_requested_cv_.NotifyOne();
  } else {
    ret = TransportAdapter::BAD_STATE;
    LOGGER_WARN(logger_, "BAD_STATE");
  }
  return ret;
}

BluetoothDeviceScanner::BluetoothDeviceScannerDelegate::
    BluetoothDeviceScannerDelegate(BluetoothDeviceScanner* scanner)
    : scanner_(scanner) {}

void BluetoothDeviceScanner::BluetoothDeviceScannerDelegate::threadMain() {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK(scanner_);
  scanner_->Thread();
}

}  // namespace transport_adapter
}  // namespace transport_manager
