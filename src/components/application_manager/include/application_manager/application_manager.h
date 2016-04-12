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

#ifndef SRC_COMPONENTS_APPLICATION_MANAGER_INCLUDE_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
#define SRC_COMPONENTS_APPLICATION_MANAGER_INCLUDE_APPLICATION_MANAGER_APPLICATION_MANAGER_H_

#include <string>
#include <vector>
#include <set>

#include "application_manager/application.h"
#include "application_manager/hmi_capabilities.h"
#include "utils/data_accessor.h"
#include "utils/shared_ptr.h"

// Other compomnents class declaration
namespace hmi_message_handler {
class HMIMessageHandler;
}
namespace protocol_handler {
class ProtocolHandler;
}
namespace connection_handler {
class ConnectionHandler;
}

namespace application_manager {

class Application;

struct ApplicationsAppIdSorter {
  bool operator()(const ApplicationSharedPtr lhs,
                  const ApplicationSharedPtr rhs) const {
    return lhs->app_id() < rhs->app_id();
  }
};
typedef std::set<ApplicationSharedPtr, ApplicationsAppIdSorter> ApplicationSet;

// typedef for Applications list iterator
typedef ApplicationSet::iterator ApplicationSetIt;

// typedef for Applications list const iterator
typedef ApplicationSet::const_iterator ApplicationSetConstIt;

class ApplicationManager {
 public:
  virtual ~ApplicationManager() {}

  /**
   * Inits application manager
   */
  virtual bool Init() = 0;

  /**
   * @brief Stop work.
   *
   * @return TRUE on success otherwise FALSE.
   **/
  virtual bool Stop() = 0;

  virtual void set_hmi_message_handler(
      hmi_message_handler::HMIMessageHandler* handler) = 0;
  virtual void set_protocol_handler(
      protocol_handler::ProtocolHandler* handler) = 0;
  virtual void set_connection_handler(
      connection_handler::ConnectionHandler* handler) = 0;

  virtual DataAccessor<ApplicationSet> applications() const = 0;
  virtual ApplicationSharedPtr application(uint32_t app_id) const = 0;
};

}  // namespace application_manager

#endif  // SRC_COMPONENTS_APPLICATION_MANAGER_INCLUDE_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
