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
#include "score/mw/com/impl/plumbing/runtime_binding_factory.h"
#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include "score/mw/com/message_passing/receiver_factory.h"
#include "score/mw/com/message_passing/receiver_mock.h"

#include "score/concurrency/long_running_threads_container.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace score::mw::com::impl
{

using ::testing::_;
using ::testing::An;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::Return;

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString1 = InstanceSpecifier::Create("/bla/blub/specifier1").value();
const auto kInstanceSpecifierString2 = InstanceSpecifier::Create("/bla/blub/specifier2").value();
ConfigurationStore kConfigStoreQm1{
    kInstanceSpecifierString1,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{1U},
};
ConfigurationStore kConfigStoreQm2{
    kInstanceSpecifierString2,
    make_ServiceIdentifierType("foo"),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{2U},
};

class ReceiverFactoryMockGuard
{
  public:
    ReceiverFactoryMockGuard()
    {
        message_passing::ReceiverFactory::InjectReceiverMock(&receiver_mock_);
    }

    ~ReceiverFactoryMockGuard()
    {

        message_passing::ReceiverFactory::InjectReceiverMock(nullptr);
    }

    message_passing::ReceiverMock receiver_mock_{};
};

class RuntimeBindingFactoryFixture : public ::testing::Test
{
  public:
    RuntimeBindingFactoryFixture& WithAConfigurationContainingOneLolaBinding()
    {
        Configuration::ServiceTypeDeployments service_type_deployments{
            {kConfigStoreQm1.service_identifier_, *kConfigStoreQm1.service_type_deployment_},
        };
        Configuration::ServiceInstanceDeployments service_instance_deployments{
            {kConfigStoreQm1.instance_specifier_, *kConfigStoreQm1.service_instance_deployment_},
        };
        configuration_ = std::make_unique<Configuration>(std::move(service_type_deployments),
                                                         std::move(service_instance_deployments),
                                                         GlobalConfiguration{},
                                                         TracingConfiguration{});
        return *this;
    }

    RuntimeBindingFactoryFixture& WithAConfigurationContainingTwoLolaBindings()
    {
        Configuration::ServiceTypeDeployments service_type_deployments{
            {kConfigStoreQm1.service_identifier_, *kConfigStoreQm1.service_type_deployment_},
            {kConfigStoreQm2.service_identifier_, *kConfigStoreQm2.service_type_deployment_},
        };
        Configuration::ServiceInstanceDeployments service_instance_deployments{
            {kConfigStoreQm1.instance_specifier_, *kConfigStoreQm1.service_instance_deployment_},
            {kConfigStoreQm2.instance_specifier_, *kConfigStoreQm2.service_instance_deployment_},
        };
        configuration_ = std::make_unique<Configuration>(std::move(service_type_deployments),
                                                         std::move(service_instance_deployments),
                                                         GlobalConfiguration{},
                                                         TracingConfiguration{});
        return *this;
    }

    RuntimeBindingFactoryFixture& WithAConfigurationContainingOneBlankBinding()
    {
        const auto instance_identifier = dummy_instance_identifier_builder_.CreateBlankBindingInstanceIdentifier();
        Configuration::ServiceTypeDeployments service_type_deployments{
            {kConfigStoreQm1.service_identifier_,
             InstanceIdentifierView{instance_identifier}.GetServiceTypeDeployment()},
        };
        Configuration::ServiceInstanceDeployments service_instance_deployments{
            {kConfigStoreQm1.instance_specifier_,
             InstanceIdentifierView{instance_identifier}.GetServiceInstanceDeployment()},
        };
        configuration_ = std::make_unique<Configuration>(std::move(service_type_deployments),
                                                         std::move(service_instance_deployments),
                                                         GlobalConfiguration{},
                                                         TracingConfiguration{});
        return *this;
    }

    // creation of a LoLa runtime will lead to the creation of a message-passing-facade, which will directly from its
    // ctor register some MessageReceiveCallbacks and start listening, so we inject a receiver mock into the factory
    ReceiverFactoryMockGuard receiver_mock_guard_{};

    concurrency::LongRunningThreadsContainer long_running_threads_{};
    std::unique_ptr<Configuration> configuration_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(RuntimeBindingFactoryFixture, CanCreateLolaBinding)
{
    // Given a configuration containing a single lola binding
    WithAConfigurationContainingOneLolaBinding();

    // When calling CreateBindingRuntimes
    auto runtimes = RuntimeBindingFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then a single lola runtime binding will be created
    EXPECT_EQ(runtimes.size(), 1);
    const auto& lola_runtime = runtimes[BindingType::kLoLa];
    EXPECT_EQ(lola_runtime->GetBindingType(), BindingType::kLoLa);
}

TEST_F(RuntimeBindingFactoryFixture, WillOnlyCreateABindingRuntimeFromTheFirstLolaConfigurationThatIsFound)
{
    // Given a configuration containing a single lola binding
    WithAConfigurationContainingTwoLolaBindings();

    // When calling CreateBindingRuntimes
    auto runtimes = RuntimeBindingFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then a single lola runtime binding will be created
    EXPECT_EQ(runtimes.size(), 1);
    const auto& lola_runtime = runtimes[BindingType::kLoLa];
    EXPECT_EQ(lola_runtime->GetBindingType(), BindingType::kLoLa);
}

TEST_F(RuntimeBindingFactoryFixture, CannotCreateBlankBinding)
{
    // Given a configuration containing a single blank binding
    WithAConfigurationContainingOneBlankBinding();

    // When calling CreateBindingRuntimes
    auto runtimes = RuntimeBindingFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then no runtime binding will be created
    EXPECT_TRUE(runtimes.empty());
}

}  // namespace score::mw::com::impl
