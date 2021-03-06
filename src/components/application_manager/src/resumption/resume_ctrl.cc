/*
 Copyright (c) 2015, Ford Motor Company
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
#include "application_manager/resumption/resume_ctrl.h"

#include <fstream>
#include <algorithm>
#include <functional>

#include "application_manager/application_manager_impl.h"

#include "config_profile/profile.h"
#include "utils/file_system.h"
#include "connection_handler/connection_handler_impl.h"
#include "application_manager/message_helper.h"
#include "connection_handler/connection.h"
#include "application_manager/commands/command_impl.h"
#include "resumption/last_state.h"
#include "policy/policy_manager_impl.h"
#include "application_manager/policies/policy_handler.h"
#include "utils/helpers.h"
#include "application_manager/resumption/resumption_data_db.h"
#include "application_manager/resumption/resumption_data_json.h"

namespace resumption {
using namespace application_manager;

CREATE_LOGGERPTR_GLOBAL(logger_, "ResumeCtrl")

ResumeCtrl::ResumeCtrl()
    : queue_lock_(false)
    , restore_hmi_level_timer_(
          "RsmCtrlRstore", this, &ResumeCtrl::ApplicationResumptiOnTimer)
    , save_persistent_data_timer_(
          "RsmCtrlPercist", this, &ResumeCtrl::SaveDataOnTimer, true)
    , is_resumption_active_(false)
    , is_data_saved_(false)
    , launch_time_(time(NULL)) {}

bool ResumeCtrl::Init() {
  using namespace profile;
  bool use_db = Profile::instance()->use_db_for_resumption();
  if (use_db) {
    resumption_storage_.reset(new ResumptionDataDB());
    if (!resumption_storage_->Init()) {
      return false;
    }

    ResumptionDataDB* db =
        dynamic_cast<ResumptionDataDB*>(resumption_storage_.get());

    if (!db->IsDBVersionActual()) {
      LOGGER_INFO(logger_,
                  "DB version had been changed. "
                  "Rebuilding resumption DB.");

      smart_objects::SmartObject data;
      db->GetAllData(data);

      if (!db->RefreshDB()) {
        return false;
      }

      db->SaveAllData(data);
      db->UpdateDBVersion();
    }
  } else {
    resumption_storage_.reset(new ResumptionDataJson());
  }
  LoadResumeData();
  save_persistent_data_timer_.start(
      profile::Profile::instance()
          ->app_resumption_save_persistent_data_timeout());
  return true;
}

ResumeCtrl::~ResumeCtrl() {}

void ResumeCtrl::SaveAllApplications() {
  ApplicationManagerImpl::ApplicationListAccessor accessor;
  std::for_each(accessor.GetData().begin(),
                accessor.GetData().end(),
                std::bind1st(std::mem_fun(&ResumeCtrl::SaveApplication), this));
}

void ResumeCtrl::SaveApplication(ApplicationSharedPtr application) {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN_VOID(application);
  LOGGER_INFO(logger_,
              "application with appID " << application->app_id()
                                        << " will be saved");
  resumption_storage_->SaveApplication(application);
}

void ResumeCtrl::on_event(const event_engine::Event& event) {
  LOGGER_DEBUG(logger_, "Event received" << event.id());
}

bool ResumeCtrl::RestoreAppHMIState(ApplicationSharedPtr application) {
  using namespace mobile_apis;
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(application, false);
  LOGGER_DEBUG(logger_,
               "app_id : " << application->app_id() << "; policy_app_id : "
                           << application->mobile_app_id());
  const std::string device_id =
      MessageHelper::GetDeviceMacAddressForHandle(application->device());
  smart_objects::SmartObject saved_app(smart_objects::SmartType_Map);
  bool result = resumption_storage_->GetSavedApplication(
      application->mobile_app_id(), device_id, saved_app);
  if (result) {
    DCHECK_OR_RETURN(application, false);
    if (saved_app.keyExists(strings::hmi_level)) {
      const HMILevel::eType saved_hmi_level =
          static_cast<mobile_apis::HMILevel::eType>(
              saved_app[strings::hmi_level].asInt());
      LOGGER_DEBUG(logger_, "Saved HMI Level is : " << saved_hmi_level);
      return SetAppHMIState(application, saved_hmi_level, true);
    } else {
      result = false;
      LOGGER_ERROR(logger_, "saved app data corrupted");
    }
  } else {
    LOGGER_ERROR(logger_, "Application not saved");
  }
  return result;
}

bool ResumeCtrl::SetupDefaultHMILevel(ApplicationSharedPtr application) {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(application, false);
  mobile_apis::HMILevel::eType default_hmi =
      ApplicationManagerImpl::instance()->GetDefaultHmiLevel(application);
  return SetAppHMIState(application, default_hmi, false);
}

void ResumeCtrl::ApplicationResumptiOnTimer() {
  LOGGER_AUTO_TRACE(logger_);
  sync_primitives::AutoLock auto_lock(queue_lock_);
  WaitingForTimerList::iterator it = waiting_for_timer_.begin();

  for (; it != waiting_for_timer_.end(); ++it) {
    ApplicationSharedPtr app =
        ApplicationManagerImpl::instance()->application(*it);
    if (!app) {
      LOGGER_ERROR(logger_, "Invalid app_id = " << *it);
      continue;
    }
    StartAppHmiStateResumption(app);
  }
  is_resumption_active_ = false;
  waiting_for_timer_.clear();
}

void ResumeCtrl::OnAppActivated(ApplicationSharedPtr application) {
  if (is_resumption_active_) {
    RemoveFromResumption(application->app_id());
  }
}

void ResumeCtrl::RemoveFromResumption(uint32_t app_id) {
  LOGGER_AUTO_TRACE(logger_);
  queue_lock_.Acquire();
  waiting_for_timer_.remove(app_id);
  queue_lock_.Release();
}

bool ResumeCtrl::SetAppHMIState(ApplicationSharedPtr application,
                                const mobile_apis::HMILevel::eType hmi_level,
                                bool check_policy) {
  using namespace mobile_apis;
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(application, false);
  LOGGER_TRACE(logger_,
               " app_id : " << application->app_id()
                            << ", hmi_level : " << hmi_level
                            << ", check_policy : " << check_policy);
  const std::string device_id =
      MessageHelper::GetDeviceMacAddressForHandle(application->device());
  if (check_policy &&
      policy::PolicyHandler::instance()->GetUserConsentForDevice(device_id) !=
          policy::DeviceConsent::kDeviceAllowed) {
    LOGGER_ERROR(logger_, "Resumption abort. Data consent wasn't allowed");
    SetupDefaultHMILevel(application);
    return false;
  }
  application->set_is_resuming(true);
  ApplicationManagerImpl::instance()->SetState(application->app_id(),
                                               hmi_level);
  LOGGER_INFO(logger_,
              "Application with policy id " << application->mobile_app_id()
                                            << " got HMI level " << hmi_level);
  return true;
}

bool ResumeCtrl::IsHMIApplicationIdExist(uint32_t hmi_app_id) {
  LOGGER_DEBUG(logger_, "hmi_app_id :" << hmi_app_id);
  return resumption_storage_->IsHMIApplicationIdExist(hmi_app_id);
}

bool ResumeCtrl::IsApplicationSaved(const std::string& policy_app_id,
                                    const std::string& device_id) {
  return -1 !=
         resumption_storage_->IsApplicationSaved(policy_app_id, device_id);
}

uint32_t ResumeCtrl::GetHMIApplicationID(const std::string& policy_app_id,
                                         const std::string& device_id) const {
  return resumption_storage_->GetHMIApplicationID(policy_app_id, device_id);
}

bool ResumeCtrl::RemoveApplicationFromSaved(
    ApplicationConstSharedPtr application) {
  const std::string device_id =
      MessageHelper::GetDeviceMacAddressForHandle(application->device());
  return resumption_storage_->RemoveApplicationFromSaved(
      application->mobile_app_id(), device_id);
}

void ResumeCtrl::OnSuspend() {
  LOGGER_AUTO_TRACE(logger_);
  StopSavePersistentDataTimer();
  SaveAllApplications();
  return resumption_storage_->OnSuspend();
}

void ResumeCtrl::OnAwake() {
  return resumption_storage_->OnAwake();
  ResetLaunchTime();
  StartSavePersistentDataTimer();
}

void ResumeCtrl::StartSavePersistentDataTimer() {
  LOGGER_AUTO_TRACE(logger_);
  if (!save_persistent_data_timer_.isRunning()) {
    save_persistent_data_timer_.start(
        profile::Profile::instance()
            ->app_resumption_save_persistent_data_timeout());
  }
}

void ResumeCtrl::StopSavePersistentDataTimer() {
  LOGGER_AUTO_TRACE(logger_);
  if (save_persistent_data_timer_.isRunning()) {
    save_persistent_data_timer_.stop();
  }
}

bool ResumeCtrl::StartResumption(ApplicationSharedPtr application,
                                 const std::string& hash) {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(application, false);
  LOGGER_DEBUG(
      logger_,
      "Resume app_id = " << application->app_id()
                         << " hmi_app_id = " << application->hmi_app_id()
                         << " policy_id = " << application->mobile_app_id()
                         << " received hash = '" << hash << "'");
  smart_objects::SmartObject saved_app;
  bool result = resumption_storage_->GetSavedApplication(
      application->mobile_app_id(),
      MessageHelper::GetDeviceMacAddressForHandle(application->device()),
      saved_app);
  if (!result) {
    LOGGER_DEBUG(logger_, "Application not found in the resumption storage");
  }
  if (result) {
    const std::string saved_hash = saved_app[strings::hash_id].asString();
    LOGGER_DEBUG(logger_, "Saved hash: " << saved_hash);
    result = saved_hash == hash ? RestoreApplicationData(application) : false;
    application->UpdateHash();
    AddToResumptionTimerQueue(application->app_id());
  }
  return result;
}

bool ResumeCtrl::StartResumptionOnlyHMILevel(ApplicationSharedPtr application) {
  LOGGER_AUTO_TRACE(logger_);
  if (!application) {
    LOGGER_WARN(logger_, "Application does not exist.");
    return false;
  }
  LOGGER_DEBUG(logger_,
               "HMI level resumption requested for application id "
                   << application->app_id() << "with hmi_app_id "
                   << application->hmi_app_id() << ", policy_app_id "
                   << application->mobile_app_id());
  smart_objects::SmartObject saved_app;
  bool result = resumption_storage_->GetSavedApplication(
      application->mobile_app_id(),
      MessageHelper::GetDeviceMacAddressForHandle(application->device()),
      saved_app);
  if (result) {
    AddToResumptionTimerQueue(application->app_id());
  }
  LOGGER_INFO(logger_, "StartResumptionOnlyHMILevel::Result = " << result);
  return result;
}

void ResumeCtrl::StartAppHmiStateResumption(ApplicationSharedPtr application) {
  using namespace profile;
  using namespace date_time;
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN_VOID(application);
  smart_objects::SmartObject saved_app;
  bool result = resumption_storage_->GetSavedApplication(
      application->mobile_app_id(),
      MessageHelper::GetDeviceMacAddressForHandle(application->device()),
      saved_app);
  DCHECK_OR_RETURN_VOID(result);
  const uint32_t ign_off_count = saved_app[strings::ign_off_count].asUInt();
  bool restore_data_allowed = false;
  restore_data_allowed =
      CheckAppRestrictions(application, saved_app) &&
      ((0 == ign_off_count) || CheckIgnCycleRestrictions(saved_app));
  if (restore_data_allowed) {
    LOGGER_INFO(logger_, "Resume application " << application->mobile_app_id());
    RestoreAppHMIState(application);
    RemoveApplicationFromSaved(application);
  } else {
    LOGGER_INFO(logger_,
                "Do not need to resume application "
                    << application->mobile_app_id());
  }
}

void ResumeCtrl::ResetLaunchTime() {
  launch_time_ = time(NULL);
}

bool ResumeCtrl::CheckPersistenceFilesForResumption(
    ApplicationSharedPtr application) {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(application, false);
  LOGGER_DEBUG(logger_,
               "Resume app_id = " << application->app_id() << " policy_id = "
                                  << application->mobile_app_id());
  smart_objects::SmartObject saved_app;
  bool result = resumption_storage_->GetSavedApplication(
      application->mobile_app_id(),
      MessageHelper::GetDeviceMacAddressForHandle(application->device()),
      saved_app);
  if (result) {
    if (!CheckIcons(application, saved_app[strings::application_commands])) {
      return false;
    }
    if (!CheckIcons(application, saved_app[strings::application_choice_sets])) {
      return false;
    }
  }
  return true;
}

bool ResumeCtrl::CheckApplicationHash(ApplicationSharedPtr application,
                                      const std::string& hash) {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(application, false);
  LOGGER_DEBUG(logger_,
               "app_id : " << application->app_id() << " hash : " << hash);
  smart_objects::SmartObject saved_app;
  bool result = resumption_storage_->GetSavedApplication(
      application->mobile_app_id(),
      MessageHelper::GetDeviceMacAddressForHandle(application->device()),
      saved_app);
  return result ? saved_app[strings::hash_id].asString() == hash : false;
}

void ResumeCtrl::SaveDataOnTimer() {
  LOGGER_AUTO_TRACE(logger_);
  if (is_resumption_active_) {
    LOGGER_WARN(logger_, "Resumption timer is active skip saving");
    return;
  }

  if (false == is_data_saved_) {
    SaveAllApplications();
    is_data_saved_ = true;
    if (!(profile::Profile::instance()->use_db_for_resumption())) {
      resumption::LastState::instance()->SaveToFileSystem();
    }
  }
}

bool ResumeCtrl::IsDeviceMacAddressEqual(ApplicationSharedPtr application,
                                         const std::string& saved_device_mac) {
  const std::string device_mac =
      MessageHelper::GetDeviceMacAddressForHandle(application->device());
  return device_mac == saved_device_mac;
}

bool ResumeCtrl::RestoreApplicationData(ApplicationSharedPtr application) {
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(application, false);
  LOGGER_DEBUG(logger_, "app_id : " << application->app_id());

  smart_objects::SmartObject saved_app(smart_objects::SmartType_Map);
  bool result = resumption_storage_->GetSavedApplication(
      application->mobile_app_id(),
      MessageHelper::GetDeviceMacAddressForHandle(application->device()),
      saved_app);
  if (result) {
    if (saved_app.keyExists(strings::grammar_id)) {
      const uint32_t app_grammar_id = saved_app[strings::grammar_id].asUInt();
      application->set_grammar_id(app_grammar_id);
      AddFiles(application, saved_app);
      AddSubmenues(application, saved_app);
      AddCommands(application, saved_app);
      AddChoicesets(application, saved_app);
      SetGlobalProperties(application, saved_app);
      AddSubscriptions(application, saved_app);
      result = true;
    } else {
      LOGGER_WARN(logger_,
                  "Saved data of application does not contain grammar_id");
      result = false;
    }
  } else {
    LOGGER_WARN(logger_, "Application not saved");
  }
  return result;
}

void ResumeCtrl::AddFiles(ApplicationSharedPtr application,
                          const smart_objects::SmartObject& saved_app) {
  LOGGER_AUTO_TRACE(logger_);

  if (saved_app.keyExists(strings::application_files)) {
    const smart_objects::SmartObject& application_files =
        saved_app[strings::application_files];
    for (size_t i = 0; i < application_files.length(); ++i) {
      const smart_objects::SmartObject& file_data = application_files[i];
      const bool is_persistent =
          file_data.keyExists(strings::persistent_file) &&
          file_data[strings::persistent_file].asBool();
      if (is_persistent) {
        AppFile file;
        file.is_persistent = is_persistent;
        file.is_download_complete =
            file_data[strings::is_download_complete].asBool();
        file.file_name = file_data[strings::sync_file_name].asString();
        file.file_type = static_cast<mobile_apis::FileType::eType>(
            file_data[strings::file_type].asInt());
        application->AddFile(file);
      }
    }
  } else {
    LOGGER_FATAL(logger_, "application_files section is not exists");
  }
}

void ResumeCtrl::AddSubmenues(ApplicationSharedPtr application,
                              const smart_objects::SmartObject& saved_app) {
  LOGGER_AUTO_TRACE(logger_);

  if (saved_app.keyExists(strings::application_submenus)) {
    const smart_objects::SmartObject& app_submenus =
        saved_app[strings::application_submenus];
    for (size_t i = 0; i < app_submenus.length(); ++i) {
      const smart_objects::SmartObject& submenu = app_submenus[i];
      application->AddSubMenu(submenu[strings::menu_id].asUInt(), submenu);
    }
    ProcessHMIRequests(
        MessageHelper::CreateAddSubMenuRequestToHMI(application));
  } else {
    LOGGER_FATAL(logger_, "application_submenus section is not exists");
  }
}

void ResumeCtrl::AddCommands(ApplicationSharedPtr application,
                             const smart_objects::SmartObject& saved_app) {
  LOGGER_AUTO_TRACE(logger_);

  if (saved_app.keyExists(strings::application_commands)) {
    const smart_objects::SmartObject& app_commands =
        saved_app[strings::application_commands];
    for (size_t i = 0; i < app_commands.length(); ++i) {
      const smart_objects::SmartObject& command = app_commands[i];

      application->AddCommand(command[strings::cmd_id].asUInt(), command);
    }
    ProcessHMIRequests(
        MessageHelper::CreateAddCommandRequestToHMI(application));
  } else {
    LOGGER_FATAL(logger_, "application_commands section is not exists");
  }
}

void ResumeCtrl::AddChoicesets(ApplicationSharedPtr application,
                               const smart_objects::SmartObject& saved_app) {
  LOGGER_AUTO_TRACE(logger_);

  if (saved_app.keyExists(strings::application_choice_sets)) {
    const smart_objects::SmartObject& app_choice_sets =
        saved_app[strings::application_choice_sets];
    for (size_t i = 0; i < app_choice_sets.length(); ++i) {
      const smart_objects::SmartObject& choice_set = app_choice_sets[i];
      const int32_t choice_set_id =
          choice_set[strings::interaction_choice_set_id].asInt();
      application->AddChoiceSet(choice_set_id, choice_set);
    }
    ProcessHMIRequests(
        MessageHelper::CreateAddVRCommandRequestFromChoiceToHMI(application));
  } else {
    LOGGER_FATAL(logger_, "There is no any choicesets");
  }
}

void ResumeCtrl::SetGlobalProperties(
    ApplicationSharedPtr application,
    const smart_objects::SmartObject& saved_app) {
  LOGGER_AUTO_TRACE(logger_);

  if (saved_app.keyExists(strings::application_global_properties)) {
    const smart_objects::SmartObject& properties_so =
        saved_app[strings::application_global_properties];
    application->load_global_properties(properties_so);
    MessageHelper::SendGlobalPropertiesToHMI(application);
  }
}

void ResumeCtrl::AddSubscriptions(ApplicationSharedPtr application,
                                  const smart_objects::SmartObject& saved_app) {
  LOGGER_AUTO_TRACE(logger_);
  if (saved_app.keyExists(strings::application_subscribtions)) {
    const smart_objects::SmartObject& subscribtions =
        saved_app[strings::application_subscribtions];

    if (subscribtions.keyExists(strings::application_buttons)) {
      const smart_objects::SmartObject& subscribtions_buttons =
          subscribtions[strings::application_buttons];
      mobile_apis::ButtonName::eType btn;
      for (size_t i = 0; i < subscribtions_buttons.length(); ++i) {
        btn = static_cast<mobile_apis::ButtonName::eType>(
            (subscribtions_buttons[i]).asInt());
        application->SubscribeToButton(btn);
      }
    }
    MessageHelper::SendAllOnButtonSubscriptionNotificationsForApp(application);

    if (subscribtions.keyExists(strings::application_vehicle_info)) {
      const smart_objects::SmartObject& subscribtions_ivi =
          subscribtions[strings::application_vehicle_info];
      VehicleDataType ivi;
      for (size_t i = 0; i < subscribtions_ivi.length(); ++i) {
        ivi = static_cast<VehicleDataType>((subscribtions_ivi[i]).asInt());
        application->SubscribeToIVI(ivi);
      }
    }
    ProcessHMIRequests(MessageHelper::GetIVISubscriptionRequests(application));
  }
}

bool ResumeCtrl::CheckIgnCycleRestrictions(
    const smart_objects::SmartObject& saved_app) {
  LOGGER_AUTO_TRACE(logger_);
  bool result = true;

  if (!CheckDelayAfterIgnOn()) {
    LOGGER_INFO(logger_, "Application was connected long after ign on");
    result = false;
  }

  if (!DisconnectedJustBeforeIgnOff(saved_app)) {
    LOGGER_INFO(logger_, "Application was dissconnected long before ign off");
    result = false;
  }
  return result;
}

bool ResumeCtrl::DisconnectedJustBeforeIgnOff(
    const smart_objects::SmartObject& saved_app) {
  using namespace date_time;
  using namespace profile;
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(saved_app.keyExists(strings::time_stamp), false);

  const time_t time_stamp =
      static_cast<time_t>(saved_app[strings::time_stamp].asUInt());
  time_t ign_off_time =
      static_cast<time_t>(resumption_storage_->GetIgnOffTime());
  const uint32_t sec_spent_before_ign = labs(ign_off_time - time_stamp);
  LOGGER_DEBUG(logger_,
               "ign_off_time "
                   << ign_off_time << "; app_disconnect_time " << time_stamp
                   << "; sec_spent_before_ign " << sec_spent_before_ign
                   << "; resumption_delay_before_ign "
                   << Profile::instance()->resumption_delay_before_ign());
  return sec_spent_before_ign <=
         Profile::instance()->resumption_delay_before_ign();
}

bool ResumeCtrl::CheckAppRestrictions(
    ApplicationConstSharedPtr application,
    const smart_objects::SmartObject& saved_app) {
  using namespace mobile_apis;
  using namespace helpers;
  LOGGER_AUTO_TRACE(logger_);
  DCHECK_OR_RETURN(saved_app.keyExists(strings::hmi_level), false);

  const bool is_media_app = application->is_media_application();
  const HMILevel::eType hmi_level =
      static_cast<HMILevel::eType>(saved_app[strings::hmi_level].asInt());
  const bool result = Compare<HMILevel::eType, EQ, ONE>(
                          hmi_level, HMILevel::HMI_FULL, HMILevel::HMI_LIMITED)
                          ? true
                          : false;
  LOGGER_DEBUG(logger_,
               "is_media_app " << is_media_app << "; hmi_level " << hmi_level
                               << " result " << result);
  return result;
}

bool ResumeCtrl::CheckIcons(ApplicationSharedPtr application,
                            smart_objects::SmartObject& obj) {
  using namespace smart_objects;
  LOGGER_AUTO_TRACE(logger_);
  const mobile_apis::Result::eType verify_images =
      MessageHelper::VerifyImageFiles(obj, application);
  return mobile_apis::Result::INVALID_DATA != verify_images;
}

bool ResumeCtrl::CheckDelayAfterIgnOn() {
  using namespace date_time;
  using namespace profile;
  LOGGER_AUTO_TRACE(logger_);
  const time_t curr_time = time(NULL);
  const time_t sdl_launch_time = launch_time();
  const uint32_t seconds_from_sdl_start = labs(curr_time - sdl_launch_time);
  const uint32_t wait_time = Profile::instance()->resumption_delay_after_ign();
  LOGGER_DEBUG(logger_,
               "curr_time " << curr_time << "; sdl_launch_time "
                            << sdl_launch_time << "; seconds_from_sdl_start "
                            << seconds_from_sdl_start << "; wait_time "
                            << wait_time);
  return seconds_from_sdl_start <= wait_time;
}

time_t ResumeCtrl::launch_time() const {
  return launch_time_;
}

time_t ResumeCtrl::GetIgnOffTime() {
  return resumption_storage_->GetIgnOffTime();
}

bool ResumeCtrl::ProcessHMIRequest(smart_objects::SmartObjectSPtr request,
                                   bool use_events) {
  LOGGER_AUTO_TRACE(logger_);
  if (use_events) {
    const hmi_apis::FunctionID::eType function_id =
        static_cast<hmi_apis::FunctionID::eType>(
            (*request)[strings::function_id].asInt());

    const int32_t hmi_correlation_id =
        (*request)[strings::correlation_id].asInt();
    subscribe_on_event(function_id, hmi_correlation_id);
  }
  if (!ApplicationManagerImpl::instance()->ManageHMICommand(request)) {
    LOGGER_ERROR(logger_, "Unable to send request");
    return false;
  }
  return true;
}

void ResumeCtrl::ProcessHMIRequests(
    const smart_objects::SmartObjectList& requests) {
  for (smart_objects::SmartObjectList::const_iterator it = requests.begin(),
                                                      total = requests.end();
       it != total;
       ++it) {
    ProcessHMIRequest(*it, true);
  }
}

void ResumeCtrl::AddToResumptionTimerQueue(uint32_t app_id) {
  LOGGER_AUTO_TRACE(logger_);
  queue_lock_.Acquire();
  waiting_for_timer_.push_back(app_id);
  queue_lock_.Release();
  if (!is_resumption_active_) {
    is_resumption_active_ = true;
    restore_hmi_level_timer_.start(
        profile::Profile::instance()->app_resuming_timeout());
  }
}

void ResumeCtrl::LoadResumeData() {
  LOGGER_AUTO_TRACE(logger_);
  smart_objects::SmartObject so_applications_data;
  resumption_storage_->GetDataForLoadResumeData(so_applications_data);
  size_t length = so_applications_data.length();
  smart_objects::SmartObject* full_app = NULL;
  smart_objects::SmartObject* limited_app = NULL;
  time_t time_stamp_full = 0;
  time_t time_stamp_limited = 0;
  // only apps with first IGN should be resumed
  const int32_t first_ign = 1;
  for (size_t i = 0; i < length; ++i) {
    if (first_ign == so_applications_data[i][strings::ign_off_count].asInt()) {
      const mobile_apis::HMILevel::eType saved_hmi_level =
          static_cast<mobile_apis::HMILevel::eType>(
              so_applications_data[i][strings::hmi_level].asInt());
      const time_t saved_time_stamp = static_cast<time_t>(
          so_applications_data[i][strings::time_stamp].asUInt());
      if (mobile_apis::HMILevel::HMI_FULL == saved_hmi_level) {
        if (time_stamp_full < saved_time_stamp) {
          time_stamp_full = saved_time_stamp;
          full_app = &(so_applications_data[i]);
        }
      }
      if (mobile_apis::HMILevel::HMI_LIMITED == saved_hmi_level) {
        if (time_stamp_limited < saved_time_stamp) {
          time_stamp_limited = saved_time_stamp;
          limited_app = &(so_applications_data[i]);
        }
      }
    }
    // set invalid HMI level for all
    resumption_storage_->UpdateHmiLevel(
        so_applications_data[i][strings::app_id].asString(),
        so_applications_data[i][strings::device_id].asString(),
        static_cast<int32_t>(mobile_apis::HMILevel::INVALID_ENUM));
  }
  if (full_app != NULL) {
    resumption_storage_->UpdateHmiLevel(
        (*full_app)[strings::app_id].asString(),
        (*full_app)[strings::device_id].asString(),
        static_cast<int32_t>(mobile_apis::HMILevel::HMI_FULL));
  }
  if (limited_app != NULL) {
    resumption_storage_->UpdateHmiLevel(
        (*limited_app)[strings::app_id].asString(),
        (*limited_app)[strings::device_id].asString(),
        static_cast<int32_t>(mobile_apis::HMILevel::HMI_LIMITED));
  }
}

}  // namespce resumption
