/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/bindings/lola/runtime.h"

#include "score/os/mocklib/unistdmock.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/message_passing/receiver_factory.h"
#include "score/mw/com/message_passing/receiver_mock.h"

#include "score/concurrency/long_running_threads_container.h"

#include "gmock/gmock.h"
#include <gtest/gtest.h>
#include <memory>
#include <set>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::_;
using ::testing::An;
using ::testing::AnyNumber;

const auto kInstanceSpecifier = InstanceSpecifier::Create("abc/abc/TirePressurePort").value();
const auto kInstanceSpecifier2 = InstanceSpecifier::Create("abc/abc/TirePressurePort2").value();
constexpr pid_t kOurPid = 4444;
constexpr pid_t kOurUid = 112;

class RuntimeFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        /// \todo QNX tests crashing, it happens when creating \c Runtime object by
        /// SetUp() roughly at Receiver<>::StartListening call. Preliminary, caused by thread racing, e.i. if update the
        /// kConcurrency = 1 the tests will pass. To abstract from the complex logic the solution is to provide Receiver
        /// mock. Nontheless, the mock hides the issue, but unit test logic becomes pure. Hence, the crash must be
        /// covered within component tests (or whatever) in follow up ticket. Currently, trying to avoid any impact on
        /// production logic the Receiver mock is being injected. In fact, the better approach should be implemented as
        /// follows: introduce stand alone parameter for passing it to \c Runtime constructor by pointer/reference and
        /// assingning it to `MessagePassingFacade& Runtime::lola_messaging_`, having that the mocked object could be
        /// passed directly

        // mock receiver creation for unit_ member
        score::mw::com::message_passing::ReceiverFactory::InjectReceiverMock(&receiver_mock_);

        using ShortMessageReceivedCallback = score::mw::com::message_passing::IReceiver::ShortMessageReceivedCallback;
        using MediumMessageReceivedCallback = score::mw::com::message_passing::IReceiver::MediumMessageReceivedCallback;

        EXPECT_CALL(receiver_mock_, Register(_, An<ShortMessageReceivedCallback>())).Times(AnyNumber());
        EXPECT_CALL(receiver_mock_, Register(_, An<MediumMessageReceivedCallback>())).Times(AnyNumber());
        EXPECT_CALL(receiver_mock_, StartListening)
            .WillRepeatedly(::testing::Return(score::cpp::expected_blank<score::os::Error>{}));

        // mock unistd creation for unit_ member
        score::os::MockGuard<score::os::UnistdMock> unistd_mock{};
        EXPECT_CALL(*unistd_mock, getpid()).WillRepeatedly(::testing::Return(kOurPid));
        EXPECT_CALL(*unistd_mock, getuid()).WillRepeatedly(::testing::Return(kOurUid));

        SetConfig(Configuration::ServiceTypeDeployments{},
                  Configuration::ServiceInstanceDeployments{},
                  GlobalConfiguration{},
                  TracingConfiguration{});
    }

    void TearDown() override
    {
        unit_.reset(nullptr);
        config_.reset(nullptr);
        score::mw::com::message_passing::ReceiverFactory::InjectReceiverMock(nullptr);  // unmock receiver
    }

    void SetConfig(Configuration::ServiceTypeDeployments service_types,
                   Configuration::ServiceInstanceDeployments service_instances,
                   GlobalConfiguration global_configuration,
                   TracingConfiguration tracing_configuration)
    {
        unit_.reset(nullptr);
        config_.reset(nullptr);
        config_ = std::make_unique<Configuration>(
            service_types, service_instances, std::move(global_configuration), std::move(tracing_configuration));

        unit_ = std::make_unique<Runtime>(*config_, long_running_threads_, nullptr);
    }

    score::mw::com::message_passing::ReceiverMock receiver_mock_;
    std::unique_ptr<Configuration> config_;
    concurrency::LongRunningThreadsContainer long_running_threads_;
    std::unique_ptr<Runtime> unit_;
};

TEST_F(RuntimeFixture, EnsureBindingTypeIsLoLa)
{

    EXPECT_EQ(unit_->GetBindingType(), score::mw::com::impl::BindingType::kLoLa);
}

TEST_F(RuntimeFixture, EnsureCorrectAsilQMSupport)
{
    GlobalConfiguration global_configuration{};
    global_configuration.SetProcessAsilLevel(QualityType::kASIL_QM);

    SetConfig(Configuration::ServiceTypeDeployments{},
              Configuration::ServiceInstanceDeployments{},
              std::move(global_configuration),
              TracingConfiguration{});

    EXPECT_FALSE(unit_->HasAsilBSupport());
}

TEST_F(RuntimeFixture, EnsureCorrectAsilBSupport)
{
    GlobalConfiguration global_configuration{};
    global_configuration.SetProcessAsilLevel(QualityType::kASIL_B);

    SetConfig(Configuration::ServiceTypeDeployments{},
              Configuration::ServiceInstanceDeployments{},
              std::move(global_configuration),
              TracingConfiguration{});

    EXPECT_TRUE(unit_->HasAsilBSupport());
}

TEST_F(RuntimeFixture, CanRetrieveMessagingAPI)
{
    score::cpp::ignore = unit_->GetLolaMessaging();
}

TEST_F(RuntimeFixture, GetMessagePassingCfgWithPredefinedTwoLolaServiceConfig)
{
    // Given a configuration with 2 LoLa service instance deployments each with a certain set of
    // allowed consumers and producers for ASIL_QM and ASIL_B
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment1;
    lolaServiceInstanceDeployment1.allowed_consumer_.insert({QualityType::kASIL_QM, {42, 43}});
    lolaServiceInstanceDeployment1.allowed_consumer_.insert({QualityType::kASIL_B, {54, 55}});
    lolaServiceInstanceDeployment1.allowed_provider_.insert({QualityType::kASIL_QM, {15}});
    lolaServiceInstanceDeployment1.allowed_provider_.insert({QualityType::kASIL_B, {15}});
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment2;
    lolaServiceInstanceDeployment2.allowed_consumer_.insert({QualityType::kASIL_QM, {42, 60}});
    lolaServiceInstanceDeployment2.allowed_consumer_.insert({QualityType::kASIL_B, {42, 60}});
    lolaServiceInstanceDeployment2.allowed_provider_.insert({QualityType::kASIL_QM, {55}});
    lolaServiceInstanceDeployment2.allowed_provider_.insert({QualityType::kASIL_B, {56}});
    ServiceInstanceDeployment::BindingInformation binding1(lolaServiceInstanceDeployment1);
    ServiceInstanceDeployment::BindingInformation binding2(lolaServiceInstanceDeployment2);

    ServiceIdentifierType si1 = make_ServiceIdentifierType("foo", 1U, 1U);
    ServiceIdentifierType si2 = make_ServiceIdentifierType("bar", 1U, 1U);
    ServiceInstanceDeployment deployment1(si1, binding1, QualityType::kASIL_B, kInstanceSpecifier);
    ServiceInstanceDeployment deployment2(si2, binding2, QualityType::kASIL_QM, kInstanceSpecifier2);

    auto instance_specifier_result = InstanceSpecifier::Create("foo_1");
    ASSERT_TRUE(instance_specifier_result.has_value());
    auto instance_specifier_result_2 = InstanceSpecifier::Create("bar_1");
    ASSERT_TRUE(instance_specifier_result_2.has_value());

    Configuration::ServiceInstanceDeployments instanceDeployments;
    instanceDeployments.insert({instance_specifier_result.value(), deployment1});
    instanceDeployments.insert({instance_specifier_result_2.value(), deployment2});
    GlobalConfiguration global_configuration{};
    global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_QM, 5);
    global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_B, 7);
    global_configuration.SetProcessAsilLevel(QualityType::kASIL_B);

    Configuration configuration{Configuration::ServiceTypeDeployments{},
                                instanceDeployments,
                                std::move(global_configuration),
                                TracingConfiguration{}};

    // when creating a LoLa runtime with this configuration
    Runtime unit{configuration, long_running_threads_, nullptr};
    // and reading out the ASIL_QM and ASIL_B specific message passing cfgs
    MessagePassingFacade::AsilSpecificCfg cfg_qm = unit.GetMessagePassingCfg(QualityType::kASIL_QM);
    MessagePassingFacade::AsilSpecificCfg cfg_b = unit.GetMessagePassingCfg(QualityType::kASIL_B);

    // expect that the queue sizes correspond to the sizes set in the configuration
    EXPECT_EQ(cfg_qm.message_queue_rx_size_, 5);
    EXPECT_EQ(cfg_b.message_queue_rx_size_, 7);

    // expect that the user_ids allowed as senders for the QM receiver are the aggregation over all service instance
    // deployments of all allowed consumers/providers for QM
    std::set<uid_t> expected_userids_qm = {15, 42, 43, 55, 60};
    EXPECT_EQ(cfg_qm.allowed_user_ids_.size(), expected_userids_qm.size());
    for (auto id : cfg_qm.allowed_user_ids_)
    {
        EXPECT_EQ(expected_userids_qm.count(id), 1);
    }

    // expect that the user_ids allowed as senders for the B receiver are the aggregation over all service instance
    // deployments of all allowed consumers/providers for B
    std::set<uid_t> expected_userids_b = {15, 42, 54, 55, 56, 60};
    EXPECT_EQ(cfg_b.allowed_user_ids_.size(), expected_userids_b.size());
    for (auto id : cfg_b.allowed_user_ids_)
    {
        EXPECT_EQ(expected_userids_b.count(id), 1);
    }
}

TEST_F(RuntimeFixture, GetMessagePassingCfgOneEmptyQMProvider)
{
    // Given a configuration with 2 LoLa service instance deployments each with a certain set of
    // allowed consumers and producers for ASIL_QM and ASIL_B (one QM provider set empty)
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment1;
    lolaServiceInstanceDeployment1.allowed_consumer_.insert({QualityType::kASIL_QM, {42, 43}});
    lolaServiceInstanceDeployment1.allowed_consumer_.insert({QualityType::kASIL_B, {54, 55}});
    lolaServiceInstanceDeployment1.allowed_provider_.insert({QualityType::kASIL_QM, {15}});
    lolaServiceInstanceDeployment1.allowed_provider_.insert({QualityType::kASIL_B, {15}});
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment2;
    lolaServiceInstanceDeployment2.allowed_consumer_.insert({QualityType::kASIL_QM, {42, 60}});
    lolaServiceInstanceDeployment2.allowed_consumer_.insert({QualityType::kASIL_B, {42, 60}});
    lolaServiceInstanceDeployment2.allowed_provider_.insert({QualityType::kASIL_QM, {}});
    lolaServiceInstanceDeployment2.allowed_provider_.insert({QualityType::kASIL_B, {56}});
    ServiceInstanceDeployment::BindingInformation binding1(lolaServiceInstanceDeployment1);
    ServiceInstanceDeployment::BindingInformation binding2(lolaServiceInstanceDeployment2);

    ServiceIdentifierType si1 = make_ServiceIdentifierType("foo", 1U, 1U);
    ServiceIdentifierType si2 = make_ServiceIdentifierType("bar", 1U, 1U);
    ServiceInstanceDeployment deployment1(si1, binding1, QualityType::kASIL_B, kInstanceSpecifier);
    ServiceInstanceDeployment deployment2(si2, binding2, QualityType::kASIL_QM, kInstanceSpecifier2);

    auto instance_specifier_result = InstanceSpecifier::Create("foo_1");
    ASSERT_TRUE(instance_specifier_result.has_value());
    auto instance_specifier_result_2 = InstanceSpecifier::Create("bar_1");
    ASSERT_TRUE(instance_specifier_result_2.has_value());

    Configuration::ServiceInstanceDeployments instanceDeployments;
    instanceDeployments.insert({instance_specifier_result.value(), deployment1});
    instanceDeployments.insert({instance_specifier_result_2.value(), deployment2});

    GlobalConfiguration global_configuration{};
    global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_QM, 5);
    global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_B, 7);

    // ... and process ASIL level set to kASIL_B
    global_configuration.SetProcessAsilLevel(QualityType::kASIL_B);

    Configuration configuration{Configuration::ServiceTypeDeployments{},
                                instanceDeployments,
                                std::move(global_configuration),
                                TracingConfiguration{}};

    // ... and message passing queue size set to for QM and B

    // when creating a LoLa runtime with this configuration
    Runtime unit{configuration, long_running_threads_, nullptr};
    // and reading out the ASIL_QM and ASIL_B specific message passing cfgs
    MessagePassingFacade::AsilSpecificCfg cfg_qm = unit.GetMessagePassingCfg(QualityType::kASIL_QM);
    MessagePassingFacade::AsilSpecificCfg cfg_b = unit.GetMessagePassingCfg(QualityType::kASIL_B);

    // expect that the queue sizes correspond to the sizes set in the configuration
    EXPECT_EQ(cfg_qm.message_queue_rx_size_, 5);
    EXPECT_EQ(cfg_b.message_queue_rx_size_, 7);

    // expect that the user_ids allowed as senders for the QM receiver are an empty set
    // deployments of all allowed consumers/providers for QM
    EXPECT_EQ(cfg_qm.allowed_user_ids_.size(), 0);

    // expect that the user_ids allowed as senders for the B receiver are the aggregation over all service instance
    // deployments of all allowed consumers/providers for B
    std::set<uid_t> expected_userids_b = {15, 42, 54, 55, 56, 60};
    EXPECT_EQ(cfg_b.allowed_user_ids_.size(), expected_userids_b.size());
    for (auto id : cfg_b.allowed_user_ids_)
    {
        EXPECT_EQ(expected_userids_b.count(id), 1);
    }
}

TEST_F(RuntimeFixture, GetMessagePassingCfgOneEmptyQMConsumer)
{
    // Given a configuration with 2 LoLa service instance deployments each with a certain set of
    // allowed consumers and producers for ASIL_QM and ASIL_B (one QM consumer set empty)
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment1;
    lolaServiceInstanceDeployment1.allowed_consumer_.insert({QualityType::kASIL_QM, {}});
    lolaServiceInstanceDeployment1.allowed_consumer_.insert({QualityType::kASIL_B, {54, 55}});
    lolaServiceInstanceDeployment1.allowed_provider_.insert({QualityType::kASIL_QM, {15}});
    lolaServiceInstanceDeployment1.allowed_provider_.insert({QualityType::kASIL_B, {15}});
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment2;
    lolaServiceInstanceDeployment2.allowed_consumer_.insert({QualityType::kASIL_QM, {42, 60}});
    lolaServiceInstanceDeployment2.allowed_consumer_.insert({QualityType::kASIL_B, {42, 60}});
    lolaServiceInstanceDeployment2.allowed_provider_.insert({QualityType::kASIL_QM, {55}});
    lolaServiceInstanceDeployment2.allowed_provider_.insert({QualityType::kASIL_B, {56}});
    ServiceInstanceDeployment::BindingInformation binding1(lolaServiceInstanceDeployment1);
    ServiceInstanceDeployment::BindingInformation binding2(lolaServiceInstanceDeployment2);

    ServiceIdentifierType si1 = make_ServiceIdentifierType("foo", 1U, 1U);
    ServiceIdentifierType si2 = make_ServiceIdentifierType("bar", 1U, 1U);
    ServiceInstanceDeployment deployment1(si1, binding1, QualityType::kASIL_B, kInstanceSpecifier);
    ServiceInstanceDeployment deployment2(si2, binding2, QualityType::kASIL_QM, kInstanceSpecifier2);

    auto instance_specifier_result = InstanceSpecifier::Create("foo_1");
    ASSERT_TRUE(instance_specifier_result.has_value());
    auto instance_specifier_result_2 = InstanceSpecifier::Create("bar_1");
    ASSERT_TRUE(instance_specifier_result_2.has_value());

    Configuration::ServiceInstanceDeployments instanceDeployments;
    instanceDeployments.insert({instance_specifier_result.value(), deployment1});
    instanceDeployments.insert({instance_specifier_result_2.value(), deployment2});

    GlobalConfiguration global_configuration{};
    // ... and message passing queue size set to for QM and B
    global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_QM, 5);
    global_configuration.SetReceiverMessageQueueSize(QualityType::kASIL_B, 7);

    // ... and process ASIL level set to kASIL_B
    global_configuration.SetProcessAsilLevel(QualityType::kASIL_B);

    Configuration configuration{Configuration::ServiceTypeDeployments{},
                                instanceDeployments,
                                std::move(global_configuration),
                                TracingConfiguration{}};

    // when creating a LoLa runtime with this configuration
    Runtime unit{configuration, long_running_threads_, nullptr};

    // and reading out the ASIL_QM and ASIL_B specific message passing cfgs
    MessagePassingFacade::AsilSpecificCfg cfg_qm = unit.GetMessagePassingCfg(QualityType::kASIL_QM);
    MessagePassingFacade::AsilSpecificCfg cfg_b = unit.GetMessagePassingCfg(QualityType::kASIL_B);

    // expect that the queue sizes correspond to the sizes set in the configuration
    EXPECT_EQ(cfg_qm.message_queue_rx_size_, 5);
    EXPECT_EQ(cfg_b.message_queue_rx_size_, 7);

    // expect that the user_ids allowed as senders for the QM receiver are an empty set
    // deployments of all allowed consumers/providers for QM
    EXPECT_EQ(cfg_qm.allowed_user_ids_.size(), 0);

    // expect that the user_ids allowed as senders for the B receiver are the aggregation over all service instance
    // deployments of all allowed consumers/providers for B
    std::set<uid_t> expected_userids_b = {15, 42, 54, 55, 56, 60};
    EXPECT_EQ(cfg_b.allowed_user_ids_.size(), expected_userids_b.size());
    for (auto id : cfg_b.allowed_user_ids_)
    {
        EXPECT_EQ(expected_userids_b.count(id), 1);
    }
}

TEST_F(RuntimeFixture, GettingAsilBConfigInQmProcessTerminates)
{
    // Given a configuration with a LoLa service instance deployment
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment1;
    ServiceInstanceDeployment::BindingInformation binding1(lolaServiceInstanceDeployment1);

    ServiceIdentifierType si1 = make_ServiceIdentifierType("foo", 1U, 1U);
    ServiceInstanceDeployment deployment1(si1, binding1, QualityType::kASIL_B, kInstanceSpecifier);
    Configuration::ServiceInstanceDeployments instanceDeployments;
    GlobalConfiguration global_configuration{};
    // ... and process ASIL level set to kASIL_QM
    global_configuration.SetProcessAsilLevel(QualityType::kASIL_QM);

    Configuration configuration{Configuration::ServiceTypeDeployments{},
                                instanceDeployments,
                                std::move(global_configuration),
                                TracingConfiguration{}};

    // when creating a LoLa runtime with this configuration
    Runtime unit{configuration, long_running_threads_, nullptr};

    // the program terminates when reading out the ASIL_B specific message passing cfgs
    EXPECT_DEATH(unit.GetMessagePassingCfg(QualityType::kASIL_B), ".*");
}

TEST_F(RuntimeFixture, EnsureCorrectPidReturned)
{
    EXPECT_EQ(unit_->GetPid(), kOurPid);
}

TEST_F(RuntimeFixture, EnsureCorrectUidReturned)
{
    EXPECT_EQ(unit_->GetUid(), kOurUid);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
