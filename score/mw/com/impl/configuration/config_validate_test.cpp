/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/configuration/config_validate.h"

#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/tracing_configuration.h"
#include "score/mw/com/impl/instance_specifier.h"

#include <score/assert_support.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace score::mw::com::impl::configuration
{
namespace
{

constexpr auto kValidInstanceSpecifier = "abc/abc/TirePressurePort";
// An InstanceSpecifier with a trailing slash is rejected by InstanceSpecifier::Create().
constexpr auto kInvalidInstanceSpecifier = "invalid_instance_specifier/";

ServiceIdentifierType MakeServiceIdentifier(const std::string& name = "/score/ncar/services/TirePressureService")
{
    return make_ServiceIdentifierType(name, 12U, 34U);
}

LolaEventInstanceDeployment MakeEventInstanceDeployment()
{
    return LolaEventInstanceDeployment{std::nullopt, std::nullopt, std::nullopt, false, 0U};
}

Configuration MakeConfiguration(const QualityType process_asil_level)
{
    GlobalConfiguration global_configuration{};
    global_configuration.SetProcessAsilLevel(process_asil_level);
    return Configuration{{}, {}, std::move(global_configuration), TracingConfiguration{}};
}

// ---------------------------------------------------------------------------
// EmplaceOrFatal
// ---------------------------------------------------------------------------
TEST(ConfigValidateEmplaceOrFatal, EmplacingNewKeyInsertsElement)
{
    std::unordered_map<std::string, int> map{};

    EmplaceOrFatal(map, std::string{"element"}, 42, "An element");

    ASSERT_EQ(map.size(), 1U);
    EXPECT_EQ(map.at("element"), 42);
}

TEST(ConfigValidateEmplaceOrFatalDeathTest, EmplacingDuplicateKeyTerminates)
{
    std::unordered_map<std::string, int> map{};
    EmplaceOrFatal(map, std::string{"element"}, 42, "An element");

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(EmplaceOrFatal(map, std::string{"element"}, 43, "An element"));
}

// ---------------------------------------------------------------------------
// ValidateUniqueServiceElementIds
// ---------------------------------------------------------------------------
TEST(ConfigValidateUniqueServiceElementIds, UniqueIdsAcrossEventsFieldsMethodsPass)
{
    const LolaServiceTypeDeployment deployment{
        LolaServiceId{1234U}, {{"event_a", 1U}, {"event_b", 2U}}, {{"field_a", 3U}}, {{"method_a", 4U}}};

    ValidateUniqueServiceElementIds(deployment);

    SUCCEED();
}

TEST(ConfigValidateUniqueServiceElementIdsDeathTest, DuplicateIdWithinEventsTerminates)
{
    const LolaServiceTypeDeployment deployment{LolaServiceId{1234U}, {{"event_a", 1U}, {"event_b", 1U}}, {}, {}};

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ValidateUniqueServiceElementIds(deployment));
}

TEST(ConfigValidateUniqueServiceElementIdsDeathTest, DuplicateIdAcrossEventAndFieldTerminates)
{
    const LolaServiceTypeDeployment deployment{LolaServiceId{1234U}, {{"event_a", 1U}}, {{"field_a", 1U}}, {}};

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ValidateUniqueServiceElementIds(deployment));
}

// ---------------------------------------------------------------------------
// CreateValidInstanceSpecifier
// ---------------------------------------------------------------------------
TEST(ConfigValidateCreateValidInstanceSpecifier, ValidNameReturnsMatchingSpecifier)
{
    const auto instance_specifier = CreateValidInstanceSpecifier(kValidInstanceSpecifier);

    EXPECT_EQ(instance_specifier.ToString(), kValidInstanceSpecifier);
}

TEST(ConfigValidateCreateValidInstanceSpecifierDeathTest, InvalidNameTerminates)
{
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore =
                                                          CreateValidInstanceSpecifier(kInvalidInstanceSpecifier));
}

// ---------------------------------------------------------------------------
// ValidateSingleDeployment
// ---------------------------------------------------------------------------
TEST(ConfigValidateSingleDeployment, ExactlyOneDeploymentPasses)
{
    const std::vector<int> deployments{42};

    ValidateSingleDeployment(deployments, MakeServiceIdentifier());

    SUCCEED();
}

TEST(ConfigValidateSingleDeploymentDeathTest, EmptyDeploymentsTerminate)
{
    const std::vector<int> deployments{};

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ValidateSingleDeployment(deployments, MakeServiceIdentifier()));
}

TEST(ConfigValidateSingleDeploymentDeathTest, MultipleDeploymentsTerminate)
{
    const std::vector<int> deployments{1, 2};

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(ValidateSingleDeployment(deployments, MakeServiceIdentifier()));
}

// ---------------------------------------------------------------------------
// CrosscheckAsilLevels
// ---------------------------------------------------------------------------
TEST(ConfigValidateCrosscheckAsilLevels, InstanceAsilNotHigherThanProcessPasses)
{
    auto config = MakeConfiguration(QualityType::kASIL_B);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = CreateValidInstanceSpecifier(kValidInstanceSpecifier);
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier,
                                  LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                  QualityType::kASIL_B,
                                  instance_specifier});

    CrosscheckAsilLevels(config);

    SUCCEED();
}

TEST(ConfigValidateCrosscheckAsilLevelsDeathTest, InstanceAsilHigherThanProcessTerminates)
{
    auto config = MakeConfiguration(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = CreateValidInstanceSpecifier(kValidInstanceSpecifier);
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier,
                                  LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                  QualityType::kASIL_B,
                                  instance_specifier});

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(CrosscheckAsilLevels(config));
}

// ---------------------------------------------------------------------------
// CrosscheckServiceInstancesToTypes
// ---------------------------------------------------------------------------
TEST(ConfigValidateCrosscheckServiceInstancesToTypes, MatchingInstanceAndTypeWithMatchingEventPasses)
{
    auto config = MakeConfiguration(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = CreateValidInstanceSpecifier(kValidInstanceSpecifier);

    config.AddServiceTypeDeployment(
        service_identifier,
        ServiceTypeDeployment{LolaServiceTypeDeployment{LolaServiceId{1234U}, {{"event_a", 1U}}, {}, {}}});

    LolaServiceInstanceDeployment lola_instance{LolaServiceInstanceId{1U}};
    lola_instance.events_.emplace("event_a", MakeEventInstanceDeployment());
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier, lola_instance, QualityType::kASIL_QM, instance_specifier});

    CrosscheckServiceInstancesToTypes(config);

    SUCCEED();
}

TEST(ConfigValidateCrosscheckServiceInstancesToTypesDeathTest, InstanceReferencingUnknownServiceTypeTerminates)
{
    auto config = MakeConfiguration(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = CreateValidInstanceSpecifier(kValidInstanceSpecifier);

    // No matching ServiceTypeDeployment is added for service_identifier.
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier,
                                  LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                  QualityType::kASIL_QM,
                                  instance_specifier});

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(CrosscheckServiceInstancesToTypes(config));
}

TEST(ConfigValidateCrosscheckServiceInstancesToTypesDeathTest, InstanceEventNotInServiceTypeTerminates)
{
    auto config = MakeConfiguration(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = CreateValidInstanceSpecifier(kValidInstanceSpecifier);

    // Service type deployment does not contain "event_a".
    config.AddServiceTypeDeployment(service_identifier,
                                    ServiceTypeDeployment{LolaServiceTypeDeployment{LolaServiceId{1234U}, {}, {}, {}}});

    LolaServiceInstanceDeployment lola_instance{LolaServiceInstanceId{1U}};
    lola_instance.events_.emplace("event_a", MakeEventInstanceDeployment());
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier, lola_instance, QualityType::kASIL_QM, instance_specifier});

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(CrosscheckServiceInstancesToTypes(config));
}

}  // namespace
}  // namespace score::mw::com::impl::configuration
