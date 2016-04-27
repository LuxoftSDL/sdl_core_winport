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

#include "gtest/gtest.h"
#include "transport_manager/tcp/tcp_device.h"
#include "transport_manager/transport_adapter/device.h"

namespace test {
namespace components {
namespace transport_manager_test {

using namespace ::transport_manager;
using namespace ::transport_manager::transport_adapter;

class TestDevice : public Device {
 public:
  TestDevice(const uint32_t& in_addr, const std::string& name)
      : Device(name, name), in_addr_(in_addr) {}
  bool IsSameAs(const Device* other_device) const {
    return true;
  }
  ApplicationList GetApplicationList() const {
    ApplicationList app_list;
    return app_list;
  }
  const uint32_t in_addr_;
};

TEST(TcpDeviceTest, CompareWithOtherTCPDevice) {
	utils::HostAddress host_address;
  uint32_t in_addr = 10;
  std::string name = "tcp_device";
  TcpDevice test_tcp_device(host_address, name);
  TcpDevice other(host_address, "other");

  EXPECT_TRUE(test_tcp_device.IsSameAs(&other));
}

TEST(TcpDeviceTest, CompareWithOtherNotTCPDevice) {
	utils::HostAddress host_address;
  uint32_t in_addr = 10;
  std::string name = "tcp_device";
  TcpDevice test_tcp_device(host_address, name);
  TestDevice other(in_addr, "other");

  EXPECT_FALSE(test_tcp_device.IsSameAs(&other));
}

TEST(TcpDeviceTest, AddApplications) {
	utils::HostAddress host_address;
  uint32_t in_addr = 1;
  std::string name = "tcp_device";

  TcpDevice test_tcp_device(host_address, name);

  int port = 12345;

  test_tcp_device.AddApplication(12345, false);
  test_tcp_device.AddApplication(23456, true);

  ApplicationList applist = test_tcp_device.GetApplicationList();
  ASSERT_EQ(2u, applist.size());
  EXPECT_EQ(1, applist[0]);
  EXPECT_EQ(2, applist[1]);

  EXPECT_EQ(port, test_tcp_device.GetApplicationPort(applist[0]));
  // Because incoming = true
  EXPECT_EQ(-1, test_tcp_device.GetApplicationPort(applist[1]));
}

}  // namespace transport_manager_test
}  // namespace components
}  // namespace test