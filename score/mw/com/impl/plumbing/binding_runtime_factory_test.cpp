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
#include "score/mw/com/impl/plumbing/binding_runtime_factory.h"
#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"

#include "score/concurrency/long_running_threads_container.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace score::mw::com::impl
{

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString1 = InstanceSpecifier::Create(std::string{"/bla/blub/specifier1"}).value();
const auto kInstanceSpecifierString2 = InstanceSpecifier::Create(std::string{"/bla/blub/specifier2"}).value();
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

class BindingRuntimeFactoryFixture : public ::testing::Test
{
  public:
    BindingRuntimeFactoryFixture& WithAConfigurationContainingOneLolaBinding()
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

    BindingRuntimeFactoryFixture& WithAConfigurationContainingTwoLolaBindings()
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

    BindingRuntimeFactoryFixture& WithAConfigurationContainingOneBlankBinding()
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

    BindingRuntimeFactoryFixture& WithAConfigurationContainingOneSomeIpBinding()
    {
        const ServiceTypeDeployment someip_type_deployment{SomeIpServiceTypeDeployment{kServiceId}};
        const ServiceInstanceDeployment someip_instance_deployment{
            make_ServiceIdentifierType("foo"),
            SomeIpServiceInstanceDeployment{SomeIpServiceInstanceId{1U}},
            QualityType::kASIL_QM,
            kInstanceSpecifierString1};

        Configuration::ServiceTypeDeployments service_type_deployments{
            {kConfigStoreQm1.service_identifier_, someip_type_deployment},
        };
        Configuration::ServiceInstanceDeployments service_instance_deployments{
            {kConfigStoreQm1.instance_specifier_, someip_instance_deployment},
        };
        configuration_ = std::make_unique<Configuration>(std::move(service_type_deployments),
                                                         std::move(service_instance_deployments),
                                                         GlobalConfiguration{},
                                                         TracingConfiguration{});
        return *this;
    }

    concurrency::LongRunningThreadsContainer long_running_threads_{};
    std::unique_ptr<Configuration> configuration_{nullptr};
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(BindingRuntimeFactoryFixture, CanCreateLolaBinding)
{
    // Given a configuration containing a single lola binding
    WithAConfigurationContainingOneLolaBinding();

    // When calling CreateBindingRuntimes
    auto runtimes = BindingRuntimeFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then a single lola runtime binding will be created
    EXPECT_EQ(runtimes.size(), 1);
    const auto& lola_runtime = runtimes[BindingType::kLoLa];
    EXPECT_EQ(lola_runtime->GetBindingType(), BindingType::kLoLa);
}

TEST_F(BindingRuntimeFactoryFixture, WillOnlyCreateABindingRuntimeFromTheFirstLolaConfigurationThatIsFound)
{
    // Given a configuration containing a single lola binding
    WithAConfigurationContainingTwoLolaBindings();

    // When calling CreateBindingRuntimes
    auto runtimes = BindingRuntimeFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then a single lola runtime binding will be created
    EXPECT_EQ(runtimes.size(), 1);
    const auto& lola_runtime = runtimes[BindingType::kLoLa];
    EXPECT_EQ(lola_runtime->GetBindingType(), BindingType::kLoLa);
}

TEST_F(BindingRuntimeFactoryFixture, CannotCreateBlankBinding)
{
    // Given a configuration containing a single blank binding
    WithAConfigurationContainingOneBlankBinding();

    // When calling CreateBindingRuntimes
    auto runtimes = BindingRuntimeFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then no runtime binding will be created
    EXPECT_TRUE(runtimes.empty());
}

TEST_F(BindingRuntimeFactoryFixture, CanCreateSomeIpBinding)
{
    // Given a configuration containing a single SOME/IP binding
    WithAConfigurationContainingOneSomeIpBinding();

    // When calling CreateBindingRuntimes
    auto runtimes = BindingRuntimeFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then a single SOME/IP runtime binding will be created
    EXPECT_EQ(runtimes.size(), 1);
    const auto& someip_runtime = runtimes[BindingType::kSomeIp];
    EXPECT_EQ(someip_runtime->GetBindingType(), BindingType::kSomeIp);
}

TEST_F(BindingRuntimeFactoryFixture, DoesNotCreateSomeIpRuntimeForAConfigurationWithoutSomeIpServices)
{
    // Given a configuration which only contains a lola binding
    WithAConfigurationContainingOneLolaBinding();

    // When calling CreateBindingRuntimes
    auto runtimes = BindingRuntimeFactory::CreateBindingRuntimes(*configuration_, long_running_threads_, {});

    // Then no SOME/IP runtime binding will be created
    EXPECT_EQ(runtimes.count(BindingType::kSomeIp), 0);
}

}  // namespace score::mw::com::impl
