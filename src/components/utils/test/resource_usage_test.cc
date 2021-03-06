/*
 * Copyright (c) 2014, Ford Motor Company
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

#include <unistd.h>
#include "gtest/gtest.h"
#include "utils/macro.h"

#include "utils/resource_usage.h"
#include "utils/file_system.h"

namespace utils {

class ResourceUsagePrivateTest : public ::testing::Test {
 protected:
  Resources res;
};

TEST_F(ResourceUsagePrivateTest, ReadStatFileTest) {
  std::string proc_buf;
  EXPECT_TRUE(res.ReadStatFile(proc_buf));
}

TEST_F(ResourceUsagePrivateTest, GetProcInfoTest) {
  Resources::PidStats pid_stat;
  EXPECT_TRUE(res.GetProcInfo(pid_stat));
}

TEST_F(ResourceUsagePrivateTest, GetMemInfoTest) {
  Resources::MemInfo mem_info;
  EXPECT_TRUE(res.GetMemInfo(mem_info));
}

TEST_F(ResourceUsagePrivateTest, GetStatPathTest_FileExists) {
  // arrange
  std::string filename = res.GetStatPath();
  // assert
  EXPECT_TRUE(file_system::FileExists(filename));
}

TEST_F(ResourceUsagePrivateTest, GetStatPathTest_ReadFile) {
  // arrange
  std::string filename = res.GetStatPath();
  std::string output;
  // assert
  EXPECT_TRUE(file_system::ReadFile(filename, output));
}
TEST_F(ResourceUsagePrivateTest, GetProcPathTest) {
  /// arrange
  std::string fd = res.GetProcPath();
  std::string filename = res.GetStatPath();
  // assert
  EXPECT_EQ(filename, fd + "/stat");
}
}

namespace test {
namespace components {
namespace utils {
using namespace ::utils;

TEST(ResourceUsageTest, SuccesfulGrabResources) {
  ResourseUsage* resources = Resources::getCurrentResourseUsage();
  EXPECT_TRUE(resources != NULL);
  delete resources;
}

}  // namespace utils
}  // namespace components
}  // namespace test
