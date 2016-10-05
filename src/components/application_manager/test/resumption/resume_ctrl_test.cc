/*
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

#include "application_manager/resumption/resume_ctrl_impl.h"
#include <string>
#include <algorithm>
#include "gtest/gtest.h"
#include "application_manager/usage_statistics.h"
#include "application_manager/mock_application.h"
#include "application_manager/mock_resumption_data.h"
#include "interfaces/MOBILE_API.h"
#include "application_manager/application_manager_impl.h"
#include "application_manager/application.h"
#include "utils/data_accessor.h"
#include "utils/make_shared.h"
#include "application_manager/mock_message_helper.h"
#include "application_manager/mock_application_manager.h"
#include "application_manager/mock_application_manager_settings.h"
#include "application_manager/mock_event_dispatcher.h"
#include "application_manager/mock_state_controller.h"

#if defined(_MSC_VER)
#define snprintf _snprintf_s
#endif

namespace test {
namespace components {
namespace resumption_test {

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::AtLeast;
using ::testing::Eq;
using namespace application_manager_test;

using namespace resumption;
using namespace mobile_apis::HMILevel;

class ResumeCtrlTest : public ::testing::Test {
 protected:
  ResumeCtrlTest()
      : kTestAppId_(10u)
      , kTestPolicyAppId_("kTestPolicyAppId_")
      , kMacAddress_("12345")
      , kDefaultTestLevel_(eType::HMI_NONE)
      , kCorId_(7u)
      , kTestDevId_(5u)
      , kTestGrammarId_(10)
      , kHash_("saved_hash")
      , kAppResumingTimeout_(30000000u)
      , kTestTimeStamp_(1452074434) {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(mock_app_manager_, event_dispatcher())
        .WillRepeatedly(ReturnRef(mock_event_dispatcher_));
    mock_storage_ =
        ::utils::MakeShared<NiceMock<resumption_test::MockResumptionData> >(
            mock_application_manager_settings_);
    app_mock_ = utils::MakeShared<NiceMock<MockApplication> >();
    res_ctrl_ = utils::MakeShared<ResumeCtrlImpl>(mock_app_manager_);
    res_ctrl_->set_resumption_storage(mock_storage_);

    EXPECT_CALL(mock_app_manager_, state_controller())
        .WillRepeatedly(ReturnRef(state_controller_));
    EXPECT_CALL(mock_app_manager_, get_settings())
        .WillRepeatedly(ReturnRef(mock_application_manager_settings_));

    ON_CALL(mock_application_manager_settings_, use_db_for_resumption())
        .WillByDefault(Return(false));
    EXPECT_CALL(mock_application_manager_settings_, app_resuming_timeout())
        .WillRepeatedly(ReturnRef(kAppResumingTimeout_));
  }

  void GetInfoFromApp() {
    ON_CALL(*app_mock_, policy_app_id())
        .WillByDefault(Return(kTestPolicyAppId_));
    ON_CALL(*app_mock_, mac_address()).WillByDefault(ReturnRef(kMacAddress_));
    EXPECT_CALL(*app_mock_, app_id()).WillRepeatedly(Return(kTestAppId_));
  }

  NiceMock<event_engine_test::MockEventDispatcher> mock_event_dispatcher_;
  application_manager_test::MockApplicationManagerSettings
      mock_application_manager_settings_;
  application_manager_test::MockApplicationManager mock_app_manager_;
  MockStateController state_controller_;
  utils::SharedPtr<ResumeCtrlImpl> res_ctrl_;
  utils::SharedPtr<NiceMock<resumption_test::MockResumptionData> >
      mock_storage_;
  utils::SharedPtr<NiceMock<MockApplication> > app_mock_;
  // app_mock.app_id() will return this value
  const uint32_t kTestAppId_;
  const std::string kTestPolicyAppId_;
  const std::string kMacAddress_;
  const mobile_apis::HMILevel::eType kDefaultTestLevel_;
  const uint32_t kCorId_;
  // app_mock.Device() will return this value
  const uint32_t kTestDevId_;
  const uint32_t kTestGrammarId_;
  const std::string kHash_;
  const uint32_t kAppResumingTimeout_;
  const uint32_t kTestTimeStamp_;
  sync_primitives::Lock app_set_lock_;
};

/**
 * @brief  Group of tests which check starting resumption with different data
 */

TEST_F(ResumeCtrlTest, StartResumption_AppWithGrammarId) {
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;

  // Check RestoreApplicationData
  GetInfoFromApp();
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(*mock_storage_,
              GetSavedApplication(kTestPolicyAppId_, kMacAddress_, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  EXPECT_CALL(*app_mock_, UpdateHash());
  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));

  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

MATCHER_P4(CheckAppFile, is_persistent, is_download, file_name, file_type, "") {
  application_manager::AppFile app_file = arg;
  return app_file.is_persistent == is_persistent &&
         app_file.is_download_complete == is_download &&
         app_file.file_name == file_name && app_file.file_type == file_type;
}

TEST_F(ResumeCtrlTest, StartResumption_WithoutGrammarId) {
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;

  GetInfoFromApp();
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  // Check RestoreApplicationData
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  EXPECT_CALL(*app_mock_, UpdateHash());
  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_)).Times(0);

  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_FALSE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithFiles) {
  GetInfoFromApp();
  smart_objects::SmartObject test_application_files;
  smart_objects::SmartObject test_file;
  const uint32_t count_of_files = 8;

  int file_types[count_of_files];
  std::string file_names[count_of_files];
  const size_t max_size = 12;
  char numb[max_size];
  for (uint32_t i = 0; i < count_of_files; i++) {
    file_types[i] = i;
    snprintf(numb, max_size, "%d", i);
    file_names[i] = "test_file" + std::string(numb);
  }

  // Should not been added
  test_file[application_manager::strings::persistent_file] = false;
  test_application_files[0] = test_file;

  for (uint32_t i = 0; i < count_of_files; ++i) {
    test_file[application_manager::strings::persistent_file] = true;
    test_file[application_manager::strings::is_download_complete] = true;
    test_file[application_manager::strings::file_type] = file_types[i];
    test_file[application_manager::strings::sync_file_name] = file_names[i];
    test_application_files[i + 1] = test_file;
  }

  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::application_files] =
      test_application_files;

  // Check RestoreApplicationData
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  EXPECT_CALL(*app_mock_, UpdateHash());
  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));
  for (uint32_t i = 0; i < count_of_files; ++i) {
    EXPECT_CALL(*app_mock_,
                AddFile(CheckAppFile(
                    true,
                    true,
                    file_names[i],
                    static_cast<mobile_apis::FileType::eType>(file_types[i]))));
  }

  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithSubmenues) {
  GetInfoFromApp();
  smart_objects::SmartObject test_application_submenues;
  smart_objects::SmartObject test_submenu;

  const uint32_t count_of_submenues = 20;
  for (uint32_t i = 0; i < count_of_submenues; ++i) {
    test_submenu[application_manager::strings::menu_id] = i;
    test_application_submenues[i] = test_submenu;
  }

  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::application_submenus] =
      test_application_submenues;

  // Check RestoreApplicationData
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));

  for (uint32_t i = 0; i < count_of_submenues; ++i) {
    EXPECT_CALL(*app_mock_, AddSubMenu(i, test_application_submenues[i]));
  }
  smart_objects::SmartObjectList requests;

  EXPECT_CALL(mock_app_manager_, GetNextHMICorrelationID())
      .WillRepeatedly(Return(kCorId_));
  EXPECT_CALL(*application_manager::MockMessageHelper::message_helper_mock(),
              CreateAddSubMenuRequestToHMI(_, kCorId_))
      .WillRepeatedly(Return(requests));

  EXPECT_CALL(*app_mock_, UpdateHash());
  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithCommands) {
  GetInfoFromApp();
  smart_objects::SmartObject test_application_commands;
  smart_objects::SmartObject test_commands;
  const uint32_t count_of_commands = 20;

  for (uint32_t i = 0; i < count_of_commands; ++i) {
    test_commands[application_manager::strings::cmd_id] = i;
    test_application_commands[i] = test_commands;
  }

  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::application_commands] =
      test_application_commands;

  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  // Check RestoreApplicationData
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  EXPECT_CALL(*app_mock_, UpdateHash());
  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));

  for (uint32_t i = 0; i < count_of_commands; ++i) {
    EXPECT_CALL(*app_mock_, AddCommand(i, test_application_commands[i]));
  }

  smart_objects::SmartObjectList requests;
  EXPECT_CALL(*application_manager::MockMessageHelper::message_helper_mock(),
              CreateAddCommandRequestToHMI(_, _))
      .WillRepeatedly(Return(requests));

  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithChoiceSet) {
  smart_objects::SmartObject application_choice_sets;
  smart_objects::SmartObject app_choice_set;

  const uint32_t count_of_choice = 10u;
  smart_objects::SmartObject choice_vector;
  smart_objects::SmartObject choice;
  const size_t max_size = 12;
  char numb[max_size];
  for (uint32_t i = 0; i < count_of_choice; ++i) {
    snprintf(numb, max_size, "%d", i);
    choice[application_manager::strings::vr_commands] =
        "VrCommand" + std::string(numb);
    choice[application_manager::strings::choice_id] = i;
    choice_vector[i] = choice;
  }
  const uint32_t count_of_choice_sets = 5;
  for (uint32_t i = 0; i < count_of_choice_sets; ++i) {
    app_choice_set[application_manager::strings::interaction_choice_set_id] = i;
    app_choice_set[application_manager::strings::choice_set] = choice_vector;
    application_choice_sets[i] = app_choice_set;
  }

  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::application_choice_sets] =
      application_choice_sets;

  // Check RestoreApplicationData
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  EXPECT_CALL(*app_mock_, UpdateHash());
  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));

  for (uint32_t i = 0; i < count_of_choice_sets; ++i) {
    EXPECT_CALL(*app_mock_, AddChoiceSet(i, application_choice_sets[i]));
  }

  smart_objects::SmartObjectList requests;
  EXPECT_CALL(*application_manager::MockMessageHelper::message_helper_mock(),
              CreateAddVRCommandRequestFromChoiceToHMI(_, _))
      .WillRepeatedly(Return(requests));

  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithGlobalProperties) {
  // Prepare Data
  smart_objects::SmartObject test_global_properties;
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::application_global_properties] =
      test_global_properties;

  // Check RestoreApplicationData
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));

  EXPECT_CALL(*application_manager::MockMessageHelper::message_helper_mock(),
              SendGlobalPropertiesToHMI(_, _));

  EXPECT_CALL(*app_mock_, load_global_properties(test_global_properties));

  EXPECT_CALL(*app_mock_, UpdateHash());
  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithSubscribeOnButtons) {
  // Prepare Data
  smart_objects::SmartObject test_subscriptions;
  smart_objects::SmartObject app_buttons;

  uint32_t count_of_buttons = 17;
  for (uint32_t i = 0; i < count_of_buttons; ++i) {
    app_buttons[i] = i;
  }

  test_subscriptions[application_manager::strings::application_buttons] =
      app_buttons;

  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::application_subscribtions] =
      test_subscriptions;

  // Check RestoreApplicationData
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));

  for (uint32_t i = 0; i < count_of_buttons; ++i) {
    EXPECT_CALL(
        *app_mock_,
        SubscribeToButton(static_cast<mobile_apis::ButtonName::eType>(i)));
  }
  EXPECT_CALL(*app_mock_, UpdateHash());

  EXPECT_CALL(*application_manager::MockMessageHelper::message_helper_mock(),
              SendAllOnButtonSubscriptionNotificationsForApp(_, _)).Times(2);

  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithSubscriptionToIVI) {
  // Prepare Data
  smart_objects::SmartObject test_subscriptions;
  smart_objects::SmartObject app_vi;

  int vtype = application_manager::VehicleDataType::GPS;
  uint32_t i = 0;
  for (; vtype < application_manager::VehicleDataType::STEERINGWHEEL;
       ++i, ++vtype) {
    app_vi[i] = vtype;
  }

  test_subscriptions[application_manager::strings::application_vehicle_info] =
      app_vi;

  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::application_subscribtions] =
      test_subscriptions;

  // Check RestoreApplicationData
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));

  for (size_t i = 0; i < app_vi.length(); ++i) {
    EXPECT_CALL(
        *app_mock_,
        SubscribeToIVI(static_cast<application_manager::VehicleDataType>(i)));
  }

  smart_objects::SmartObjectList requests;
  EXPECT_CALL(*application_manager::MockMessageHelper::message_helper_mock(),
              GetIVISubscriptionRequests(_, _))
      .WillRepeatedly(Return(requests));

  EXPECT_CALL(*app_mock_, UpdateHash());
  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumption_AppWithSubscriptionToWayPoints) {
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::subscribed_for_way_points] = true;
  saved_app[application_manager::strings::time_stamp] = kTestTimeStamp_;

  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  EXPECT_CALL(*app_mock_, set_grammar_id(kTestGrammarId_));
  EXPECT_CALL(mock_app_manager_, SubscribeAppForWayPoints(_));
  const mobile_apis::HMILevel::eType hmi_test_level =
      mobile_apis::HMILevel::HMI_FULL;
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillOnce(Return(hmi_test_level));
  EXPECT_CALL(state_controller_, SetRegularState(_, hmi_test_level));

  const bool kResult = res_ctrl_->StartResumption(app_mock_, kHash_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartResumptionOnlyHMILevel) {
  smart_objects::SmartObject saved_app;

  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillOnce(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(kTestPolicyAppId_, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  const bool kResult = res_ctrl_->StartResumptionOnlyHMILevel(app_mock_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, StartAppHmiStateResumption_AppInFull) {
  mobile_apis::HMILevel::eType restored_test_type = eType::HMI_FULL;
  const uint32_t ign_off_count = 0;
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::ign_off_count] = ign_off_count;
  saved_app[application_manager::strings::hmi_level] = restored_test_type;

  EXPECT_CALL(state_controller_, SetRegularState(_, restored_test_type))
      .Times(AtLeast(1));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(kTestPolicyAppId_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(*mock_storage_, RemoveApplicationFromSaved(_, _))
      .WillOnce(Return(true));

  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .WillRepeatedly(Return(policy::kDeviceAllowed));
  res_ctrl_->StartAppHmiStateResumption(app_mock_);
}

TEST_F(ResumeCtrlTest, StartAppHmiStateResumption_AppInBackground) {
  const uint32_t ign_off_count = 0;
  smart_objects::SmartObject saved_app;

  mobile_apis::HMILevel::eType restored_test_type = eType::HMI_BACKGROUND;
  saved_app[application_manager::strings::ign_off_count] = ign_off_count;
  saved_app[application_manager::strings::hmi_level] = restored_test_type;

  EXPECT_CALL(mock_app_manager_, state_controller()).Times(0);
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(kTestPolicyAppId_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  res_ctrl_->StartAppHmiStateResumption(app_mock_);
}

/**
 * @brief  Group of tests which check restoring resumption with different data
 */

TEST_F(ResumeCtrlTest, RestoreAppHMIState_RestoreHMILevelFull) {
  mobile_apis::HMILevel::eType restored_test_type = eType::HMI_FULL;

  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::hash_id] = kHash_;
  saved_app[application_manager::strings::grammar_id] = kTestGrammarId_;
  saved_app[application_manager::strings::hmi_level] = restored_test_type;

  EXPECT_CALL(state_controller_, SetRegularState(_, restored_test_type))
      .Times(AtLeast(1));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .WillOnce(Return(policy::kDeviceAllowed));
  EXPECT_CALL(*app_mock_, set_is_resuming(true));

  const bool kResult = res_ctrl_->RestoreAppHMIState(app_mock_);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, SetupDefaultHMILevel) {
  smart_objects::SmartObject saved_app;

  saved_app[application_manager::strings::hmi_level] = kDefaultTestLevel_;

  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillRepeatedly(Return(kDefaultTestLevel_));
  GetInfoFromApp();
  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .Times(0);

  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillOnce(Return(kDefaultTestLevel_));

  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_))
      .Times(AtLeast(1));

  res_ctrl_->SetupDefaultHMILevel(app_mock_);
}

TEST_F(ResumeCtrlTest, ApplicationResumptiOnTimer_AppInFull) {
  EXPECT_CALL(mock_app_manager_, application(_))
      .WillRepeatedly(Return(app_mock_));

  mobile_apis::HMILevel::eType restored_test_type = eType::HMI_FULL;
  const uint32_t ign_off_count = 0;
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::ign_off_count] = ign_off_count;
  saved_app[application_manager::strings::hmi_level] = restored_test_type;

  MockStateController state_controller_;
  EXPECT_CALL(mock_app_manager_, state_controller())
      .WillOnce(ReturnRef(state_controller_));
  EXPECT_CALL(state_controller_, SetRegularState(_, restored_test_type))
      .Times(AtLeast(1));
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(kTestPolicyAppId_, _, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(*mock_storage_, RemoveApplicationFromSaved(_, _))
      .WillOnce(Return(true));

  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .WillRepeatedly(Return(policy::kDeviceAllowed));
  res_ctrl_->StartAppHmiStateResumption(app_mock_);
}

/**
 * @brief group of tests which check correct SetAppHMIState
*/

TEST_F(ResumeCtrlTest, SetAppHMIState_HMINone_WithoutCheckPolicy) {
  GetInfoFromApp();

  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .Times(0);

  EXPECT_CALL(*app_mock_, set_is_resuming(true));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_))
      .Times(AtLeast(1));
  const bool kResult =
      res_ctrl_->SetAppHMIState(app_mock_, kDefaultTestLevel_, false);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, SetAppHMIState_HMILimited_WithoutCheckPolicy) {
  mobile_apis::HMILevel::eType test_type = eType::HMI_LIMITED;
  GetInfoFromApp();
  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .Times(0);

  EXPECT_CALL(*app_mock_, set_is_resuming(true));
  EXPECT_CALL(state_controller_, SetRegularState(_, test_type))
      .Times(AtLeast(1));
  const bool res = res_ctrl_->SetAppHMIState(app_mock_, test_type, false);
  EXPECT_TRUE(res);
}

TEST_F(ResumeCtrlTest, SetAppHMIState_HMIFull_WithoutCheckPolicy) {
  mobile_apis::HMILevel::eType test_type = eType::HMI_FULL;
  GetInfoFromApp();
  // GetDefaultHmiLevel should not be called
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_)).Times(0);
  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .Times(0);

  EXPECT_CALL(*app_mock_, set_is_resuming(true));
  EXPECT_CALL(state_controller_, SetRegularState(_, test_type))
      .Times(AtLeast(1));

  const bool kResult = res_ctrl_->SetAppHMIState(app_mock_, test_type, false);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, SetAppHMIState_HMIFull_WithPolicy_DevAllowed) {
  mobile_apis::HMILevel::eType test_type = eType::HMI_FULL;

  GetInfoFromApp();
  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .WillOnce(Return(policy::kDeviceAllowed));

  EXPECT_CALL(*app_mock_, set_is_resuming(true));
  EXPECT_CALL(state_controller_, SetRegularState(_, test_type))
      .Times(AtLeast(1));

  const bool kResult = res_ctrl_->SetAppHMIState(app_mock_, test_type, true);
  EXPECT_TRUE(kResult);
}

TEST_F(ResumeCtrlTest, SetAppHMIState_HMIFull_WithPolicy_DevDisallowed) {
  mobile_apis::HMILevel::eType test_type = eType::HMI_FULL;

  GetInfoFromApp();
  EXPECT_CALL(mock_app_manager_, GetUserConsentForDevice(kMacAddress_))
      .WillOnce(Return(policy::kDeviceDisallowed));

  EXPECT_CALL(*app_mock_, set_is_resuming(true));
  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillOnce(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_))
      .Times(AtLeast(1));
  const bool kResult = res_ctrl_->SetAppHMIState(app_mock_, test_type, true);
  EXPECT_FALSE(kResult);
}

TEST_F(ResumeCtrlTest, SaveAllApplications) {
  utils::SharedPtr<application_manager_test::MockApplication> test_app =
      ::utils::MakeShared<application_manager_test::MockApplication>();
  EXPECT_CALL(*test_app, app_id()).WillRepeatedly(Return(kTestAppId_));

  application_manager::ApplicationSet app_set;
  app_set.insert(test_app);

  DataAccessor<application_manager::ApplicationSet> accessor(app_set,
                                                             app_set_lock_);

  EXPECT_CALL(mock_app_manager_, applications())
      .WillRepeatedly(Return(accessor));
  EXPECT_CALL(*mock_storage_, SaveApplication(Eq(test_app)));
  res_ctrl_->SaveAllApplications();

  // Fix DataAccessor release and WinQt crash
  Mock::VerifyAndClearExpectations(&mock_app_manager_);
}

TEST_F(ResumeCtrlTest, SaveAllApplications_EmptyApplicationlist) {
  utils::SharedPtr<application_manager::Application> mock_app =
      ::utils::MakeShared<application_manager_test::MockApplication>();

  application_manager::ApplicationSet app_set;

  DataAccessor<application_manager::ApplicationSet> accessor(app_set,
                                                             app_set_lock_);

  EXPECT_CALL(mock_app_manager_, applications())
      .WillRepeatedly(Return(accessor));
  EXPECT_CALL(*mock_storage_, SaveApplication(mock_app)).Times(0);
  res_ctrl_->SaveAllApplications();

  // Fix DataAccessor release and WinQt crash
  Mock::VerifyAndClearExpectations(&mock_app_manager_);
}

TEST_F(ResumeCtrlTest, SaveApplication) {
  utils::SharedPtr<application_manager_test::MockApplication> test_app =
      ::utils::MakeShared<application_manager_test::MockApplication>();
  EXPECT_CALL(*test_app, app_id()).WillRepeatedly(Return(kTestAppId_));

  EXPECT_CALL(*mock_storage_, SaveApplication(Eq(test_app)));
  res_ctrl_->SaveApplication(test_app);
}

TEST_F(ResumeCtrlTest, OnAppActivated_ResumptionHasStarted) {
  smart_objects::SmartObject saved_app;
  GetInfoFromApp();

  EXPECT_CALL(mock_app_manager_, GetDefaultHmiLevel(_))
      .WillOnce(Return(kDefaultTestLevel_));
  EXPECT_CALL(state_controller_, SetRegularState(_, kDefaultTestLevel_));
  EXPECT_CALL(*mock_storage_, GetSavedApplication(kTestPolicyAppId_, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  ON_CALL(*app_mock_, app_id()).WillByDefault(Return(kTestAppId_));

  const bool kResult = res_ctrl_->StartResumptionOnlyHMILevel(app_mock_);
  EXPECT_TRUE(kResult);

  utils::SharedPtr<application_manager_test::MockApplication> mock_app =
      ::utils::MakeShared<application_manager_test::MockApplication>();

  EXPECT_CALL(*mock_app, app_id()).WillOnce(Return(kTestAppId_));
  res_ctrl_->OnAppActivated(mock_app);
}

TEST_F(ResumeCtrlTest, OnAppActivated_ResumptionNotActive) {
  utils::SharedPtr<application_manager_test::MockApplication> mock_app =
      ::utils::MakeShared<application_manager_test::MockApplication>();
  EXPECT_CALL(*mock_app, app_id()).Times(0);
  res_ctrl_->OnAppActivated(mock_app);
}

TEST_F(ResumeCtrlTest, IsHMIApplicationIdExist) {
  const uint32_t hmi_app_id = 10u;

  EXPECT_CALL(*mock_storage_, IsHMIApplicationIdExist(hmi_app_id))
      .WillOnce(Return(true));
  EXPECT_TRUE(res_ctrl_->IsHMIApplicationIdExist(hmi_app_id));
}

TEST_F(ResumeCtrlTest, GetHMIApplicationID) {
  const uint32_t hmi_app_id = 10u;
  const std::string kDeviceId = "test_device_id";

  EXPECT_CALL(*mock_storage_, GetHMIApplicationID(kTestPolicyAppId_, kDeviceId))
      .WillOnce(Return(hmi_app_id));
  EXPECT_EQ(hmi_app_id,
            res_ctrl_->GetHMIApplicationID(kTestPolicyAppId_, kDeviceId));
}

TEST_F(ResumeCtrlTest, IsApplicationSaved) {
  const std::string kPolicyAppId = "policy_app_id";
  const std::string kDeviceId = "device_id";

  EXPECT_CALL(*mock_storage_, IsApplicationSaved(kPolicyAppId, kDeviceId))
      .WillOnce(Return(0));
  EXPECT_TRUE(res_ctrl_->IsApplicationSaved(kPolicyAppId, kDeviceId));
}

TEST_F(ResumeCtrlTest, OnAppRegistrationStart) {
  const std::string kPolicyAppId = "policy_app_id";
  const std::string kDeviceId = "device_id";

  EXPECT_CALL(*mock_storage_, IsApplicationSaved(kPolicyAppId, kDeviceId))
      .WillOnce(Return(0));
  res_ctrl_->OnAppRegistrationStart(kPolicyAppId, kDeviceId);
}

TEST_F(ResumeCtrlTest, OnAppRegistrationEnd) {
  const uint32_t timeout = 1000u;
  EXPECT_CALL(mock_application_manager_settings_,
              app_resumption_save_persistent_data_timeout())
      .WillOnce(ReturnRef(timeout));
  res_ctrl_->OnAppRegistrationEnd();
}

TEST_F(ResumeCtrlTest, CheckPersistenceFiles_WithoutCommandAndChoiceSets) {
  const uint32_t kIgnOffCount = 0;
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::ign_off_count] = kIgnOffCount;
  saved_app[application_manager::strings::hmi_level] = HMI_FULL;

  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_TRUE(res_ctrl_->CheckPersistenceFilesForResumption(app_mock_));
}

TEST_F(ResumeCtrlTest, CheckPersistenceFilesForResumption_WithCommands) {
  smart_objects::SmartObject test_application_commands;
  const uint32_t kIgnOffCount = 0;
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::ign_off_count] = kIgnOffCount;
  saved_app[application_manager::strings::hmi_level] = HMI_FULL;
  saved_app[application_manager::strings::application_commands] =
      test_application_commands;

  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_CALL(*application_manager::MockMessageHelper::message_helper_mock(),
              VerifyImageFiles(_, _, _))
      .WillRepeatedly(Return(mobile_apis::Result::SUCCESS));

  EXPECT_TRUE(res_ctrl_->CheckPersistenceFilesForResumption(app_mock_));
}

TEST_F(ResumeCtrlTest, CheckPersistenceFilesForResumption_WithChoiceSet) {
  smart_objects::SmartObject test_choice_sets;
  const uint32_t kIgnOffCount = 0;
  smart_objects::SmartObject saved_app;
  saved_app[application_manager::strings::ign_off_count] = kIgnOffCount;
  saved_app[application_manager::strings::hmi_level] = HMI_FULL;
  saved_app[application_manager::strings::application_choice_sets] =
      test_choice_sets;

  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(_, _, _))
      .WillOnce(DoAll(SetArgReferee<2>(saved_app), Return(true)));

  EXPECT_TRUE(res_ctrl_->CheckPersistenceFilesForResumption(app_mock_));
}

TEST_F(ResumeCtrlTest, OnSuspend) {
  utils::SharedPtr<application_manager_test::MockApplication> test_app =
      ::utils::MakeShared<application_manager_test::MockApplication>();
  EXPECT_CALL(*test_app, app_id()).WillRepeatedly(Return(kTestAppId_));

  application_manager::ApplicationSet app_set;
  app_set.insert(test_app);

  DataAccessor<application_manager::ApplicationSet> accessor(app_set,
                                                             app_set_lock_);

  EXPECT_CALL(mock_app_manager_, applications())
      .WillRepeatedly(Return(accessor));
  EXPECT_CALL(*mock_storage_, SaveApplication(Eq(test_app)));

  EXPECT_CALL(*mock_storage_, OnSuspend());
  EXPECT_CALL(*mock_storage_, Persist());
  res_ctrl_->OnSuspend();

  // Fix DataAccessor release and WinQt crash
  Mock::VerifyAndClearExpectations(&mock_app_manager_);
}

TEST_F(ResumeCtrlTest, OnSuspend_EmptyApplicationlist) {
  utils::SharedPtr<application_manager::Application> mock_app =
      ::utils::MakeShared<application_manager_test::MockApplication>();

  application_manager::ApplicationSet app_set;

  DataAccessor<application_manager::ApplicationSet> accessor(app_set,
                                                             app_set_lock_);

  EXPECT_CALL(mock_app_manager_, applications())
      .WillRepeatedly(Return(accessor));
  EXPECT_CALL(*mock_storage_, SaveApplication(mock_app)).Times(0);

  EXPECT_CALL(*mock_storage_, OnSuspend());
  EXPECT_CALL(*mock_storage_, Persist());
  res_ctrl_->OnSuspend();

  // Fix DataAccessor release and WinQt crash
  Mock::VerifyAndClearExpectations(&mock_app_manager_);
}

TEST_F(ResumeCtrlTest, OnAwake) {
  const uint32_t timeout = 1000u;
  EXPECT_CALL(mock_application_manager_settings_,
              app_resumption_save_persistent_data_timeout())
      .WillOnce(ReturnRef(timeout));
  EXPECT_CALL(*mock_storage_, OnAwake());
  res_ctrl_->OnAwake();
}

TEST_F(ResumeCtrlTest, RemoveApplicationFromSaved) {
  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, RemoveApplicationFromSaved(_, _))
      .WillOnce(Return(true));
  EXPECT_TRUE(res_ctrl_->RemoveApplicationFromSaved(app_mock_));
}

TEST_F(ResumeCtrlTest, CheckApplicationHash) {
  smart_objects::SmartObject saved_app;

  const std::string test_hash = "saved_hash";
  saved_app[application_manager::strings::hash_id] = test_hash;

  GetInfoFromApp();
  EXPECT_CALL(*mock_storage_, GetSavedApplication(kTestPolicyAppId_, _, _))
      .WillRepeatedly(DoAll(SetArgReferee<2>(saved_app), Return(true)));
  EXPECT_TRUE(res_ctrl_->CheckApplicationHash(app_mock_, kHash_));
}

}  // namespace resumption_test
}  // namespace components
}  // namespace test
