/*
 Copyright (c) 2014, Ford Motor Company
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following
 disclaimer in the documentation and/or other materials provided with the
 distribution.

 Neither the name of the Ford Motor Company nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#include "policy/update_status_manager.h"
#include "policy/policy_listener.h"
#include "utils/logger.h"

namespace policy {

CREATE_LOGGERPTR_GLOBAL(logger_, "UpdateStatusManager")

UpdateStatusManager::UpdateStatusManager()
    : listener_(NULL)
    , exchange_in_progress_(false)
    , update_required_(false)
    , update_scheduled_(false)
    , exchange_pending_(false)
    , apps_search_in_progress_(false)
    , last_update_status_(policy::StatusUnknown) {
  update_status_thread_delegate_ = new UpdateThreadDelegate(this);
  thread_ = threads::CreateThread("UpdateStatusThread",
                                  update_status_thread_delegate_);
  thread_->start();
}

UpdateStatusManager::~UpdateStatusManager() {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK(update_status_thread_delegate_);
  DCHECK(thread_);
  thread_->join();
  delete update_status_thread_delegate_;
  threads::DeleteThread(thread_);
}

void UpdateStatusManager::set_listener(PolicyListener* listener) {
  listener_ = listener;
}

void UpdateStatusManager::OnUpdateSentOut(uint32_t update_timeout) {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK(update_status_thread_delegate_);
  const unsigned milliseconds_in_second = 1000;
  update_status_thread_delegate_->updateTimeOut(update_timeout *
                                                milliseconds_in_second);
  set_exchange_in_progress(true);
  set_exchange_pending(true);
  set_update_required(false);
}

void UpdateStatusManager::OnUpdateTimeoutOccurs() {
  LOGGER_AUTO_TRACE(logger_);
  set_update_required(true);
  set_exchange_in_progress(false);
  set_exchange_pending(false);
  DCHECK(update_status_thread_delegate_);
  update_status_thread_delegate_->updateTimeOut(0);  // Stop Timer
}

void UpdateStatusManager::OnValidUpdateReceived() {
  LOGGER_AUTO_TRACE(logger_);
  update_status_thread_delegate_->updateTimeOut(0);  // Stop Timer
  set_exchange_pending(false);
  set_exchange_in_progress(false);
}

void UpdateStatusManager::OnWrongUpdateReceived() {
  LOGGER_AUTO_TRACE(logger_);
  update_status_thread_delegate_->updateTimeOut(0);  // Stop Timer
  set_update_required(true);
  set_exchange_in_progress(false);
}

void UpdateStatusManager::OnResetDefaultPT(bool is_update_required) {
  LOGGER_AUTO_TRACE(logger_);
  exchange_in_progress_ = false;
  update_required_ = is_update_required;
  exchange_pending_ = false;
  last_update_status_ = policy::StatusUnknown;
}

void UpdateStatusManager::OnResetRetrySequence() {
  LOGGER_AUTO_TRACE(logger_);
  if (exchange_in_progress_) {
    set_exchange_pending(true);
  }
  set_update_required(true);
}

void UpdateStatusManager::OnNewApplicationAdded() {
  LOGGER_AUTO_TRACE(logger_);
  set_update_required(true);
}

void UpdateStatusManager::OnPolicyInit(bool is_update_required) {
  LOGGER_AUTO_TRACE(logger_);
  update_required_ = is_update_required;
}

PolicyTableStatus UpdateStatusManager::GetUpdateStatus() const {
  LOGGER_AUTO_TRACE(logger_);
  if (!exchange_in_progress_ && !exchange_pending_ && !update_required_) {
    return PolicyTableStatus::StatusUpToDate;
  }

  if (update_required_ && !exchange_in_progress_ && !exchange_pending_) {
    return PolicyTableStatus::StatusUpdateRequired;
  }

  return PolicyTableStatus::StatusUpdatePending;
}

bool UpdateStatusManager::IsUpdateRequired() const {
  return update_required_ || update_scheduled_;
}

bool UpdateStatusManager::IsUpdatePending() const {
  return exchange_pending_;
}

void UpdateStatusManager::ScheduleUpdate() {
  update_scheduled_ = true;
  update_required_ = true;
}

void UpdateStatusManager::ResetUpdateSchedule() {
  update_scheduled_ = false;
}

std::string UpdateStatusManager::StringifiedUpdateStatus() const {
  switch (GetUpdateStatus()) {
    case policy::StatusUpdatePending:
      return "UPDATING";
    case policy::StatusUpdateRequired:
      return "UPDATE_NEEDED";
    case policy::StatusUpToDate:
      return "UP_TO_DATE";
    default: { return "UNKNOWN"; }
  }
}

void policy::UpdateStatusManager::OnAppsSearchStarted() {
  LOGGER_AUTO_TRACE(logger_);
  sync_primitives::AutoLock lock(apps_search_in_progress_lock_);
  apps_search_in_progress_ = true;
}

void policy::UpdateStatusManager::OnAppsSearchCompleted() {
  LOGGER_AUTO_TRACE(logger_);
  sync_primitives::AutoLock lock(apps_search_in_progress_lock_);
  apps_search_in_progress_ = false;
}

bool policy::UpdateStatusManager::IsAppsSearchInProgress() {
  LOGGER_AUTO_TRACE(logger_);
  sync_primitives::AutoLock lock(apps_search_in_progress_lock_);
  return apps_search_in_progress_;
}

void UpdateStatusManager::CheckUpdateStatus() {
  LOGGER_AUTO_TRACE(logger_);
  policy::PolicyTableStatus status = GetUpdateStatus();
  if (listener_ && last_update_status_ != status) {
    LOGGER_INFO(logger_, "Send OnUpdateStatusChanged");
    listener_->OnUpdateStatusChanged(StringifiedUpdateStatus());
  }
  last_update_status_ = status;
}

void UpdateStatusManager::set_exchange_in_progress(bool value) {
  sync_primitives::AutoLock lock(exchange_in_progress_lock_);
  LOGGER_INFO(logger_,
              "Exchange in progress value is:" << std::boolalpha << value);
  exchange_in_progress_ = value;
  CheckUpdateStatus();
}

void UpdateStatusManager::set_exchange_pending(bool value) {
  sync_primitives::AutoLock lock(exchange_pending_lock_);
  LOGGER_INFO(logger_, "Exchange pending value is:" << std::boolalpha << value);
  exchange_pending_ = value;
  CheckUpdateStatus();
}

void UpdateStatusManager::set_update_required(bool value) {
  sync_primitives::AutoLock lock(update_required_lock_);
  LOGGER_INFO(logger_, "Update required value is:" << std::boolalpha << value);
  update_required_ = value;
  CheckUpdateStatus();
}

UpdateStatusManager::UpdateThreadDelegate::UpdateThreadDelegate(
    UpdateStatusManager* update_status_manager)
    : timeout_(0)
    , stop_flag_(false)
    , state_lock_(false)
    , update_status_manager_(update_status_manager) {
  LOGGER_INFO(logger_, "Create UpdateThreadDelegate");
}

UpdateStatusManager::UpdateThreadDelegate::~UpdateThreadDelegate() {
  LOGGER_INFO(logger_, "Delete UpdateThreadDelegate");
}

void UpdateStatusManager::UpdateThreadDelegate::threadMain() {
  LOGGER_DEBUG(logger_, "UpdateStatusManager thread started (started normal)");
  while (false == stop_flag_) {
    sync_primitives::AutoLock auto_lock(state_lock_);
    if (timeout_ > 0) {
      LOGGER_DEBUG(logger_, "Timeout is greater then 0");
      sync_primitives::ConditionalVariable::WaitStatus wait_status =
          termination_condition_.WaitFor(auto_lock, timeout_);
      if (sync_primitives::ConditionalVariable::kTimeout == wait_status) {
        if (update_status_manager_) {
          update_status_manager_->OnUpdateTimeoutOccurs();
        }
      }
    } else {
      // Time is not active, wait, while timeout will be seted,
      // or UpdateStatusManager will be deleted
      termination_condition_.Wait(auto_lock);
    }
  }
}

void UpdateStatusManager::UpdateThreadDelegate::exitThreadMain() {
  sync_primitives::AutoLock auto_lock(state_lock_);
  stop_flag_ = true;
  LOGGER_INFO(logger_, "before notify");
  termination_condition_.NotifyOne();
}

void UpdateStatusManager::UpdateThreadDelegate::updateTimeOut(
    const uint32_t timeout_ms) {
  timeout_ = timeout_ms;
  termination_condition_.NotifyOne();
}

}  // namespace policy
