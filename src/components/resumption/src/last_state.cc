/*
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

#include "resumption/last_state.h"
#include "config_profile/profile.h"
#include "utils/file_system.h"
#include "utils/logger.h"

CREATE_LOGGERPTR_GLOBAL(logger_, "LastState");

utils::json::JsonValue& resumption::LastState::dictionary() {
  return dictionary_;
}

void resumption::LastState::SaveToFileSystem() {
  LOGGER_AUTO_TRACE(logger_);
  const std::string file = profile::Profile::instance()->app_info_storage();
  const std::string str = dictionary_.ToJson();
  const std::vector<uint8_t> char_vector_pdata(str.begin(), str.end());

  DCHECK(file_system::CreateDirectoryRecursively(
      profile::Profile::instance()->app_storage_folder()));

  LOGGER_INFO(logger_, "LastState::SaveToFileSystem " << file << str);

  DCHECK(file_system::Write(file, char_vector_pdata));
}

void resumption::LastState::LoadFromFileSystem() {
  using namespace utils::json;
  const std::string file = profile::Profile::instance()->app_info_storage();
  std::string buffer;
  bool result = file_system::ReadFile(file, buffer);

  if (!result) {
    LOGGER_WARN(logger_,
                "Failed to load last state. Cannot read file " << file);
    return;
  }

  if (buffer.empty()) {
    LOGGER_DEBUG(logger_, "Buffer is empty.");
    return;
  }

  JsonValue::ParseResult parse_result = JsonValue::Parse(buffer);
  if (!parse_result.second) {
    LOGGER_WARN(logger_,
                "Failed to load last state. Cannot parse json:\n" << buffer);
    return;
  }
  dictionary_ = parse_result.first;
  LOGGER_INFO(logger_, "Valid last state was found." << dictionary_.ToJson());
  return;
}

resumption::LastState::LastState() {
  LoadFromFileSystem();
}

resumption::LastState::~LastState() {
  SaveToFileSystem();
}
