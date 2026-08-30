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
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/configuration/configuration_error.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/json/internal/model/any.h"
#include "score/json/json_writer.h"

#include <score/overload.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace
{

const LolaServiceId kServiceId{1U};
const auto kInstanceSpecifierString = InstanceSpecifier::Create(std::string{"/bla/blub/instance_specifier"}).value();
ConfigurationStore kConfigStoreQm{
    kInstanceSpecifierString,
    make_ServiceIdentifierType("/bla/blub/one", 1U, 2U),
    QualityType::kASIL_QM,
    kServiceId,
    LolaServiceInstanceId{1U},
};

}  // namespace

// ConfigurationFixture is intentionally declared outside of the anonymous namespace above (and re-opened below):
// Configuration declares "friend class ConfigurationFixture;" which refers to
// score::mw::com::impl::ConfigurationFixture. Since an anonymous namespace introduces a distinct (uniquely-named) inner
// scope, a ConfigurationFixture nested inside it would be a different, unrelated class and would NOT receive the
// granted friendship.
class ConfigurationFixture : public ::testing::Test
{
  public:
    void WithMinimalConfiguration()
    {
        Configuration::ServiceTypeDeployments type_deployments{};
        type_deployments.insert({kConfigStoreQm.service_identifier_, *kConfigStoreQm.service_type_deployment_});
        Configuration::ServiceInstanceDeployments instance_deployments{};
        instance_deployments.emplace(kConfigStoreQm.instance_specifier_, *kConfigStoreQm.service_instance_deployment_);

        unit_.emplace(std::move(type_deployments),
                      std::move(instance_deployments),
                      GlobalConfiguration{},
                      TracingConfiguration{});
    }

    void WithEmptyConfiguration()
    {
        unit_.emplace(Configuration::ServiceTypeDeployments{},
                      Configuration::ServiceInstanceDeployments{},
                      GlobalConfiguration{},
                      TracingConfiguration{});
    }

    const std::string get_path(const std::string& file_name)
    {
        const std::string default_path = "score/mw/com/impl/configuration/example/" + file_name;

        std::ifstream file(default_path);
        if (file.is_open())
        {
            file.close();
            return default_path;
        }
        else
        {
            return "external/safe_posix_platform/" + default_path;
        }
    }

    /// \brief Helper method to access the configuration's private member to compare merge results
    static Configuration::ServiceTypeDeployments GetServiceTypesSnapshot(const Configuration& configuration)
    {
        return configuration.GetServiceTypes();
    }

    /// \brief Helper method to access the configuration's private member to compare merge results
    static Configuration::ServiceInstanceDeployments GetServiceInstancesSnapshot(const Configuration& configuration)
    {
        return configuration.GetServiceInstances();
    }

    std::optional<Configuration> unit_{};
};

namespace
{

score::Result<std::string> GetStringFromJson(const json::Object& json_object)
{
    json::JsonWriter json_writer{};
    return json_writer.ToBuffer(json_object);
}

/**
 * @brief TC to test construction via two maps and specific move construction.
 */
TEST_F(ConfigurationFixture, construct)
{
    // Given a Configuration instance created on a bare minimum configuration
    WithMinimalConfiguration();

    // move construct unit2 from unit
    Configuration unit2(std::move(unit_.value()));

    // verify that unit2 really contains still valid copies
    EXPECT_EQ(unit2.GetNumberOfServiceTypes(), 1);
    EXPECT_EQ(unit2.GetNumberOfServiceInstances(), 1);
    EXPECT_TRUE(unit2.GetServiceInstanceDeployment(kConfigStoreQm.instance_specifier_).has_value());

    // verify default values of global section
    EXPECT_EQ(unit2.GetGlobalConfiguration().GetProcessAsilLevel(), QualityType::kASIL_QM);
    EXPECT_EQ(unit2.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_QM),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    EXPECT_EQ(unit2.GetGlobalConfiguration().GetReceiverMessageQueueSize(QualityType::kASIL_B),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE);
    EXPECT_EQ(unit2.GetGlobalConfiguration().GetSenderMessageQueueSize(),
              GlobalConfiguration::DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE);
}

TEST_F(ConfigurationFixture, ConfigIsCorrectlyParsedFromFile)
{
    RecordProperty("ParentRequirement", "SCR-6379815");
    RecordProperty(
        "Description",
        "All relevant configuration aspects shall be read from a JSON file and not be manipulated by the read logic.");
    RecordProperty("TestingTechnique", "Requirements-based test");
    RecordProperty("DerivationTechnique", "Analysis of requirements");
    RecordProperty("Priority", "2");

    // When parsing a json configuration file
    const auto json_path{get_path("mw_com_config.json")};
    auto config = configuration::Parse(json_path);

    // Then manually generated ServiceTypes data structures using data from config file
    const auto service_identifier_type = make_ServiceIdentifierType("/score/ncar/services/TirePressureService", 12, 34);

    const std::string service_type_name{"/score/ncar/services/TirePressureService"};
    const std::string service_event_name{"CurrentPressureFrontLeft"};
    const std::string service_field_name{"CurrentTemperatureFrontLeft"};
    const LolaServiceId service_id{1234};
    const LolaEventId lola_event_type{20};
    const LolaFieldId lola_field_type{30};
    const LolaServiceTypeDeployment::EventIdMapping service_events{{service_event_name, lola_event_type}};
    const LolaServiceTypeDeployment::FieldIdMapping service_fields{{service_field_name, lola_field_type}};
    const LolaServiceTypeDeployment manual_lola_service_type(service_id, service_events, service_fields);

    // Match ServiceTypes generated from json
    const auto& generated_service_type = config.GetServiceTypeDeployment(service_identifier_type).value().get();
    const auto* generated_lola_service_type =
        std::get_if<LolaServiceTypeDeployment>(&generated_service_type.binding_info_);
    ASSERT_NE(generated_lola_service_type, nullptr);
    EXPECT_EQ(manual_lola_service_type.service_id_, generated_lola_service_type->service_id_);
    const auto& manual_service_events = manual_lola_service_type.events_;
    const auto& manual_service_fields = manual_lola_service_type.fields_;
    const auto& generated_service_events = generated_lola_service_type->events_;
    const auto& generated_service_fields = generated_lola_service_type->fields_;
    EXPECT_EQ(manual_service_events.size(), generated_service_events.size());
    EXPECT_EQ(manual_service_fields.size(), generated_service_fields.size());
    EXPECT_EQ(manual_service_events.at(service_event_name), generated_service_events.at(service_event_name));
    EXPECT_EQ(manual_service_fields.at(service_field_name), generated_service_fields.at(service_field_name));

    // And manually generated ServiceInstances using data from config file
    const auto instance_specifier_result = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"});
    ASSERT_TRUE(instance_specifier_result.has_value());

    const std::string instance_event_name{"CurrentPressureFrontLeft"};
    const std::string instance_field_name{"CurrentTemperatureFrontLeft"};
    const std::string instance_method_name{"SetPressure"};
    const LolaServiceInstanceId instance_id{1234};
    const std::size_t shared_memory_size{10000};
    const std::size_t control_asil_b_memory_size{20000};
    const std::size_t control_qm_memory_size{30000};
    const std::uint16_t event_max_samples{50};
    const std::uint8_t event_max_subscribers{5};
    const std::uint16_t field_max_samples{60};
    const std::uint8_t field_max_subscribers{6};
    const LolaMethodInstanceDeployment::QueueSize method_queue_size{20U};
    const std::vector<uid_t> allowed_consumer_ids_qm{42, 43};
    const std::vector<uid_t> allowed_consumer_ids_b{54, 55};
    const std::vector<uid_t> allowed_provider_ids_qm{15};
    const std::vector<uid_t> allowed_provider_ids_b{15};

    const LolaEventInstanceDeployment lola_event_instance{event_max_samples, event_max_subscribers, 1U, true, 0};
    const LolaFieldInstanceDeployment lola_field_instance{
        LolaEventInstanceDeployment{field_max_samples, field_max_subscribers, 1U, true, 7}, true, true};
    const LolaMethodInstanceDeployment lola_method_instance{method_queue_size, true};

    const LolaServiceInstanceDeployment::EventInstanceMapping instance_events{
        {instance_event_name, lola_event_instance}};
    const LolaServiceInstanceDeployment::FieldInstanceMapping instance_fields{
        {instance_field_name, lola_field_instance}};
    const LolaServiceInstanceDeployment::MethodInstanceMapping instance_methods{
        {instance_method_name, lola_method_instance}};
    const std::unordered_map<QualityType, std::vector<uid_t>> allowed_consumers{
        {QualityType::kASIL_QM, allowed_consumer_ids_qm}, {QualityType::kASIL_B, allowed_consumer_ids_b}};
    const std::unordered_map<QualityType, std::vector<uid_t>> allowed_providers{
        {QualityType::kASIL_QM, allowed_provider_ids_qm}, {QualityType::kASIL_B, allowed_provider_ids_b}};

    LolaServiceInstanceDeployment binding_info(instance_id, instance_events, instance_fields, instance_methods);
    binding_info.allowed_consumer_ = allowed_consumers;
    binding_info.allowed_provider_ = allowed_providers;
    binding_info.shared_memory_size_ = shared_memory_size;
    binding_info.control_asil_b_memory_size_ = control_asil_b_memory_size;
    binding_info.control_qm_memory_size_ = control_qm_memory_size;
    binding_info.inter_vm_support_ = true;
    binding_info.inter_vm_forwarded_ = true;
    const QualityType asil_level{QualityType::kASIL_B};

    ServiceInstanceDeployment manual_service_instance(
        service_identifier_type, binding_info, asil_level, instance_specifier_result.value());

    // Match ServiceInstances generated from json
    const ServiceInstanceDeployment& generated_service_instance =
        config.GetServiceInstanceDeployment(instance_specifier_result.value()).value().get();

    auto serialized_manual_service_instance = manual_service_instance.Serialize();
    auto serialized_manual_service_instance_string = GetStringFromJson(manual_service_instance.Serialize());
    ASSERT_TRUE(serialized_manual_service_instance_string.has_value());

    auto serialized_generated_service_instance = generated_service_instance.Serialize();
    auto serialized_generated_service_instance_string = GetStringFromJson(generated_service_instance.Serialize());
    ASSERT_TRUE(serialized_generated_service_instance_string.has_value());

    EXPECT_EQ(serialized_manual_service_instance_string.value(), serialized_generated_service_instance_string.value());

    auto manual_lola_service_instance_deployment =
        std::get_if<LolaServiceInstanceDeployment>(&manual_service_instance.bindingInfo_);
    auto generated_lola_service_instance_deployment =
        std::get_if<LolaServiceInstanceDeployment>(&generated_service_instance.bindingInfo_);
    ASSERT_NE(manual_lola_service_instance_deployment, nullptr);
    ASSERT_NE(generated_lola_service_instance_deployment, nullptr);
    EXPECT_EQ(manual_lola_service_instance_deployment->instance_id_,
              generated_lola_service_instance_deployment->instance_id_);
    EXPECT_EQ(manual_lola_service_instance_deployment->shared_memory_size_,
              generated_lola_service_instance_deployment->shared_memory_size_);
    EXPECT_EQ(manual_lola_service_instance_deployment->control_asil_b_memory_size_,
              generated_lola_service_instance_deployment->control_asil_b_memory_size_);
    EXPECT_EQ(manual_lola_service_instance_deployment->control_qm_memory_size_,
              generated_lola_service_instance_deployment->control_qm_memory_size_);
    EXPECT_EQ(manual_lola_service_instance_deployment->allowed_consumer_,
              generated_lola_service_instance_deployment->allowed_consumer_);
    EXPECT_EQ(manual_lola_service_instance_deployment->allowed_provider_,
              generated_lola_service_instance_deployment->allowed_provider_);
    EXPECT_EQ(manual_lola_service_instance_deployment->events_, generated_lola_service_instance_deployment->events_);
    EXPECT_EQ(manual_lola_service_instance_deployment->fields_, generated_lola_service_instance_deployment->fields_);
    EXPECT_EQ(manual_lola_service_instance_deployment->methods_, generated_lola_service_instance_deployment->methods_);
}

TEST_F(ConfigurationFixture,
       AddingAServiceTypeDeploymentWithUniqueServiceIdentifierTypeReturnsPointerToInsertedDeployment)
{
    // Given an empty configuration
    WithEmptyConfiguration();
    // When inserting a ServiceTypeDeployment with a unique ServiceIdentifierType
    const auto* const service_type_deployment_ptr = unit_.value().AddServiceTypeDeployment(
        kConfigStoreQm.service_identifier_, *kConfigStoreQm.service_type_deployment_);

    // Then the returned ServiceTypeDeployment should be the same as the provided one
    EXPECT_EQ(service_type_deployment_ptr->ToHashString(), kConfigStoreQm.service_type_deployment_->ToHashString());
}

TEST_F(ConfigurationFixture,
       AddingAServiceInstanceDeploymentWithUniqueInstanceSpecifierReturnsPointerToInsertedDeployment)
{
    // Given an empty configuration
    WithEmptyConfiguration();

    // When inserting another ServiceInstanceDeployment with a unique InstanceSpecifier
    const auto* const service_instance_deployment_ptr = unit_.value().AddServiceInstanceDeployments(
        kConfigStoreQm.instance_specifier_, *kConfigStoreQm.service_instance_deployment_);

    // Then the returned ServiceInstanceDeployment should be the same as the provided one
    EXPECT_EQ(*service_instance_deployment_ptr, *kConfigStoreQm.service_instance_deployment_);
}

// ---------------------------------------------------------------------------
// Configuration::Validate()
// ---------------------------------------------------------------------------
namespace validate_test
{

constexpr auto kValidInstanceSpecifier = "abc/abc/TirePressurePort";

ServiceIdentifierType MakeServiceIdentifier(const std::string& name = "/score/ncar/services/TirePressureService")
{
    return make_ServiceIdentifierType(name, 12U, 34U);
}

LolaEventInstanceDeployment MakeEventInstanceDeployment()
{
    return LolaEventInstanceDeployment{std::nullopt, std::nullopt, std::nullopt, false, 0U};
}

LolaFieldInstanceDeployment MakeFieldInstanceDeployment()
{
    return LolaFieldInstanceDeployment{MakeEventInstanceDeployment(), false, false};
}

Configuration MakeConfigurationWithAsilLevel(const QualityType process_asil_level)
{
    GlobalConfiguration global_configuration{};
    global_configuration.SetProcessAsilLevel(process_asil_level);
    return Configuration{{}, {}, std::move(global_configuration), TracingConfiguration{}};
}

}  // namespace validate_test

TEST(ConfigurationCrossCheckAsilLevels, InstanceAsilNotHigherThanProcessPasses)
{
    using namespace validate_test;

    // Given a Configuration whose process ASIL level matches the ASIL level of its only service instance, and whose
    // service instance correctly references its service type
    auto config = MakeConfigurationWithAsilLevel(QualityType::kASIL_B);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = InstanceSpecifier::Create(std::string{kValidInstanceSpecifier}).value();
    config.AddServiceTypeDeployment(service_identifier,
                                    ServiceTypeDeployment{LolaServiceTypeDeployment{LolaServiceId{1234U}, {}, {}, {}}});
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier,
                                  LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                  QualityType::kASIL_B,
                                  instance_specifier});

    // When validating the configuration
    const auto result = config.Validate();

    // Then validation succeeds
    EXPECT_TRUE(result.has_value());
}

TEST(ConfigurationCrossCheckAsilLevels, InstanceAsilHigherThanProcessReturnsError)
{
    using namespace validate_test;

    // Given a Configuration whose process ASIL level is lower than the ASIL level of its only service instance
    auto config = MakeConfigurationWithAsilLevel(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = InstanceSpecifier::Create(std::string{kValidInstanceSpecifier}).value();
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier,
                                  LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                  QualityType::kASIL_B,
                                  instance_specifier});

    // When validating the configuration
    const auto result = config.Validate();

    // Then validation fails with the expected error code
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<int>(configuration_errc::configuration_invalid_asil_configuration));
}

// ---------------------------------------------------------------------------
// Configuration::Validate() - CrosscheckServiceInstancesToTypes
// ---------------------------------------------------------------------------
TEST(ConfigurationValidateCrosscheckServiceInstancesToTypes, MatchingInstanceAndTypeWithMatchingEventPasses)
{
    using namespace validate_test;

    // Given a Configuration where the service instance's event refers to an existing event of its service type
    auto config = MakeConfigurationWithAsilLevel(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = InstanceSpecifier::Create(std::string{kValidInstanceSpecifier}).value();

    config.AddServiceTypeDeployment(
        service_identifier,
        ServiceTypeDeployment{LolaServiceTypeDeployment{LolaServiceId{1234U}, {{"event_a", 1U}}, {}, {}}});

    LolaServiceInstanceDeployment lola_instance{LolaServiceInstanceId{1U}};
    lola_instance.events_.emplace("event_a", MakeEventInstanceDeployment());
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier, lola_instance, QualityType::kASIL_QM, instance_specifier});

    // When validating the configuration
    const auto result = config.Validate();

    // Then validation succeeds
    EXPECT_TRUE(result.has_value());
}

TEST(ConfigurationValidateCrosscheckServiceInstancesToTypes, InstanceReferencingUnknownServiceTypeReturnsError)
{
    using namespace validate_test;

    // Given a Configuration with a service instance which refers to a service type that isn't configured
    auto config = MakeConfigurationWithAsilLevel(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = InstanceSpecifier::Create(std::string{kValidInstanceSpecifier}).value();

    // No matching ServiceTypeDeployment is added for service_identifier.
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier,
                                  LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}},
                                  QualityType::kASIL_QM,
                                  instance_specifier});

    // When validating the configuration
    const auto result = config.Validate();

    // Then validation fails with the expected error code
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(),
              static_cast<int>(configuration_errc::configuration_invalid_type_reference_from_instance));
}

TEST(ConfigurationValidateCrosscheckServiceInstancesToTypes, InstanceEventNotInServiceTypeReturnsError)
{
    using namespace validate_test;

    // Given a Configuration where the service instance's event doesn't exist in the referenced service type
    auto config = MakeConfigurationWithAsilLevel(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = InstanceSpecifier::Create(std::string{kValidInstanceSpecifier}).value();

    // Service type deployment does not contain "event_a".
    config.AddServiceTypeDeployment(service_identifier,
                                    ServiceTypeDeployment{LolaServiceTypeDeployment{LolaServiceId{1234U}, {}, {}, {}}});

    LolaServiceInstanceDeployment lola_instance{LolaServiceInstanceId{1U}};
    lola_instance.events_.emplace("event_a", MakeEventInstanceDeployment());
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier, lola_instance, QualityType::kASIL_QM, instance_specifier});

    // When validating the configuration
    const auto result = config.Validate();

    // Then validation fails with the expected error code
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(),
              static_cast<int>(configuration_errc::configuration_invalid_event_reference_from_instance));
}

TEST(ConfigurationValidateCrosscheckServiceInstancesToTypes, InstanceFieldNotInServiceTypeReturnsError)
{
    using namespace validate_test;

    // Given a Configuration where the service instance's field doesn't exist in the referenced service type
    auto config = MakeConfigurationWithAsilLevel(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = InstanceSpecifier::Create(std::string{kValidInstanceSpecifier}).value();

    // Service type deployment does not contain "field_a".
    config.AddServiceTypeDeployment(service_identifier,
                                    ServiceTypeDeployment{LolaServiceTypeDeployment{LolaServiceId{1234U}, {}, {}, {}}});

    LolaServiceInstanceDeployment lola_instance{LolaServiceInstanceId{1U}};
    lola_instance.fields_.emplace("field_a", MakeFieldInstanceDeployment());
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier, lola_instance, QualityType::kASIL_QM, instance_specifier});

    // When validating the configuration
    const auto result = config.Validate();

    // Then validation fails with the expected error code
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(),
              static_cast<int>(configuration_errc::configuration_invalid_field_reference_from_instance));
}

TEST(ConfigurationValidateCrosscheckServiceInstancesToTypes,
     ServiceTypeWithoutLolaBindingReturnsUnsupportedTypeBindingError)
{
    using namespace validate_test;

    // Given a Configuration where the referenced service type has no (LoLa) binding at all, while the service
    // instance which references it declares an event.
    auto config = MakeConfigurationWithAsilLevel(QualityType::kASIL_QM);
    const auto service_identifier = MakeServiceIdentifier();
    const auto instance_specifier = InstanceSpecifier::Create(std::string{kValidInstanceSpecifier}).value();

    config.AddServiceTypeDeployment(service_identifier, ServiceTypeDeployment{score::cpp::blank{}});

    LolaServiceInstanceDeployment lola_instance{LolaServiceInstanceId{1U}};
    lola_instance.events_.emplace("event_a", MakeEventInstanceDeployment());
    config.AddServiceInstanceDeployments(
        instance_specifier,
        ServiceInstanceDeployment{service_identifier, lola_instance, QualityType::kASIL_QM, instance_specifier});

    // When validating the configuration
    const auto result = config.Validate();

    // Then validation fails because the referenced service type doesn't have a (LoLa) binding
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), static_cast<int>(configuration_errc::configuration_unsupported_type_binding));
}

TEST_F(ConfigurationFixture, HasLolaServiceDeploymentReturnsTrueIfLolaServiceTypeDeploymentExists)
{
    // Given a configuration containing a LolaServiceTypeDeployment
    WithMinimalConfiguration();

    // When checking if the configuration has a Lola service deployment
    const auto result = unit_.value().HasLolaServiceDeployment();

    // Then the result should be true
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

TEST_F(ConfigurationFixture, HasLolaServiceDeploymentReturnsFalseIfNoLolaServiceTypeDeploymentExists)
{
    // Given a configuration without any LolaServiceTypeDeployment
    WithEmptyConfiguration();

    // When checking if the configuration has a Lola service deployment
    const auto result = unit_.value().HasLolaServiceDeployment();

    // Then the result should be false
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());
}

TEST_F(ConfigurationFixture, GetListOfNamesOfConfiguredServicesReturnsCorrectServiceNames)
{
    // Given a configuration containing 2 ServiceTypeDeployments
    WithMinimalConfiguration();
    const auto additional_service_identifier = make_ServiceIdentifierType("/bla/blub/two", 3U, 4U);
    unit_.value().AddServiceTypeDeployment(
        additional_service_identifier,
        ServiceTypeDeployment{LolaServiceTypeDeployment{LolaServiceId{5678U}, {}, {}, {}}});

    // When calling GetListOfNamesOfConfiguredServices
    const auto service_names = unit_.value().GetServiceTypeNames();

    // Then the result should contain the correct service names
    EXPECT_EQ(service_names.size(), 2);
    EXPECT_TRUE(service_names.find("/bla/blub/one") != service_names.end());
    EXPECT_TRUE(service_names.find("/bla/blub/two") != service_names.end());
}

TEST_F(ConfigurationFixture, GetListOfNamesOfConfiguredServicesReturnsEmptySetIfNoServiceTypes)
{
    // Given a configuration without any ServiceTypeDeployments
    WithEmptyConfiguration();

    // When calling GetListOfNamesOfConfiguredServices
    const auto service_names = unit_.value().GetServiceTypeNames();

    // Then the result should be an empty set
    EXPECT_TRUE(service_names.empty());
}

TEST_F(ConfigurationFixture, GetElementNamesOfServiceTypeReturnsCorrectEventNames)
{
    // Given a configuration containing a ServiceTypeDeployment with events and fields
    WithEmptyConfiguration();
    const auto additional_service_identifier = make_ServiceIdentifierType("/bla/blub/two", 3U, 4U);
    unit_.value().AddServiceTypeDeployment(
        additional_service_identifier,
        ServiceTypeDeployment{LolaServiceTypeDeployment{
            LolaServiceId{5678U}, {{"event1", 1}, {"event2", 2}}, {{"field1", 1}, {"field2", 2}}, {}}});

    // When calling GetElementNamesOfServiceType for events
    const auto event_names = unit_.value().GetElementNamesOfServiceType("/bla/blub/two", ServiceElementType::EVENT);

    // Then the result should contain the correct event names ...
    EXPECT_EQ(event_names.size(), 2);
    EXPECT_TRUE(event_names.find("event1") != event_names.end());
    EXPECT_TRUE(event_names.find("event2") != event_names.end());
}

TEST_F(ConfigurationFixture, GetElementNamesOfServiceTypeReturnsCorrectFieldNames)
{
    // Given a configuration containing a ServiceTypeDeployment with events and fields
    WithEmptyConfiguration();
    const auto additional_service_identifier = make_ServiceIdentifierType("/bla/blub/two", 3U, 4U);
    unit_.value().AddServiceTypeDeployment(
        additional_service_identifier,
        ServiceTypeDeployment{LolaServiceTypeDeployment{
            LolaServiceId{5678U}, {{"event1", 1}, {"event2", 2}}, {{"field1", 1}, {"field2", 2}}, {}}});

    // When calling GetElementNamesOfServiceType for events
    const auto field_names = unit_.value().GetElementNamesOfServiceType("/bla/blub/two", ServiceElementType::FIELD);

    // Then the result should contain the correct field names
    EXPECT_EQ(field_names.size(), 2);
    EXPECT_TRUE(field_names.find("field1") != field_names.end());
    EXPECT_TRUE(field_names.find("field2") != field_names.end());
}

TEST_F(ConfigurationFixture, GetAggregatedAllowedUsersReturnsCorrectUserIds)
{
    // Given a configuration with 2 LoLa service instance deployments each with a certain set of
    // allowed consumers and producers for ASIL_QM and ASIL_B
    WithEmptyConfiguration();

    const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    const auto kInstanceSpecifier2 = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort2"}).value();
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

    unit_.value().AddServiceInstanceDeployments(kInstanceSpecifier, deployment1);
    unit_.value().AddServiceInstanceDeployments(kInstanceSpecifier2, deployment2);

    // When calling GetAggregatedAllowedUsers for ASIL_QM and ASIL_B
    const auto allowed_users_qm = unit_.value().GetAggregatedAllowedUsers(QualityType::kASIL_QM);
    const auto allowed_users_asil_b = unit_.value().GetAggregatedAllowedUsers(QualityType::kASIL_B);

    // Then the result should contain the correct user IDs
    EXPECT_EQ(allowed_users_qm.size(), 5);
    EXPECT_TRUE(allowed_users_qm.find(15) != allowed_users_qm.end());
    EXPECT_TRUE(allowed_users_qm.find(42) != allowed_users_qm.end());
    EXPECT_TRUE(allowed_users_qm.find(43) != allowed_users_qm.end());
    EXPECT_TRUE(allowed_users_qm.find(55) != allowed_users_qm.end());
    EXPECT_TRUE(allowed_users_qm.find(60) != allowed_users_qm.end());

    EXPECT_EQ(allowed_users_asil_b.size(), 6);
    EXPECT_TRUE(allowed_users_asil_b.find(15) != allowed_users_asil_b.end());
    EXPECT_TRUE(allowed_users_asil_b.find(42) != allowed_users_asil_b.end());
    EXPECT_TRUE(allowed_users_asil_b.find(54) != allowed_users_asil_b.end());
    EXPECT_TRUE(allowed_users_asil_b.find(55) != allowed_users_asil_b.end());
    EXPECT_TRUE(allowed_users_asil_b.find(56) != allowed_users_asil_b.end());
    EXPECT_TRUE(allowed_users_asil_b.find(60) != allowed_users_asil_b.end());
}

TEST_F(ConfigurationFixture, GetAggregatedAllowedUsersReturnsEmptySetIfNoAllowedAppsAreConfigured)
{
    // Given a configuration with 2 LoLa service instance deployments for which no allowed consumer or provider
    // apps are configured
    WithEmptyConfiguration();

    const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
    const auto kInstanceSpecifier2 = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort2"}).value();
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment1;
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment2;

    ServiceInstanceDeployment::BindingInformation binding1(lolaServiceInstanceDeployment1);
    ServiceInstanceDeployment::BindingInformation binding2(lolaServiceInstanceDeployment2);

    ServiceIdentifierType si1 = make_ServiceIdentifierType("foo", 1U, 1U);
    ServiceIdentifierType si2 = make_ServiceIdentifierType("bar", 1U, 1U);
    ServiceInstanceDeployment deployment1(si1, binding1, QualityType::kASIL_B, kInstanceSpecifier);
    ServiceInstanceDeployment deployment2(si2, binding2, QualityType::kASIL_QM, kInstanceSpecifier2);

    unit_.value().AddServiceInstanceDeployments(kInstanceSpecifier, deployment1);
    unit_.value().AddServiceInstanceDeployments(kInstanceSpecifier2, deployment2);

    // When calling GetAggregatedAllowedUsers for ASIL_QM and ASIL_B
    const auto allowed_users_qm = unit_.value().GetAggregatedAllowedUsers(QualityType::kASIL_QM);
    const auto allowed_users_asil_b = unit_.value().GetAggregatedAllowedUsers(QualityType::kASIL_B);

    // Then the resulting lists should be empty for both ASIL levels
    EXPECT_TRUE(allowed_users_qm.empty());
    EXPECT_TRUE(allowed_users_asil_b.empty());
}

TEST_F(ConfigurationFixture, GetInstancesOfServiceTypeReturnsCorrectInstanceSpecifiers)
{
    // Given a configuration with 2 LoLa service instance deployments for the same service type "foo"
    WithEmptyConfiguration();

    const auto kInstanceSpecifier1 = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort1"}).value();
    const auto kInstanceSpecifier2 = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort2"}).value();
    const auto kInstanceSpecifier3 = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort3"}).value();
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment1;
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment2;
    LolaServiceInstanceDeployment lolaServiceInstanceDeployment3;

    ServiceInstanceDeployment::BindingInformation binding1(lolaServiceInstanceDeployment1);
    ServiceInstanceDeployment::BindingInformation binding2(lolaServiceInstanceDeployment2);
    ServiceInstanceDeployment::BindingInformation binding3(lolaServiceInstanceDeployment3);

    ServiceIdentifierType si1 = make_ServiceIdentifierType("foo", 1U, 1U);
    ServiceIdentifierType si2 = make_ServiceIdentifierType("bar", 1U, 1U);
    ServiceIdentifierType si3 = make_ServiceIdentifierType("foo", 1U, 2U);
    ServiceInstanceDeployment deployment1(si1, binding1, QualityType::kASIL_B, kInstanceSpecifier1);
    ServiceInstanceDeployment deployment2(si2, binding2, QualityType::kASIL_QM, kInstanceSpecifier2);
    ServiceInstanceDeployment deployment3(si3, binding3, QualityType::kASIL_B, kInstanceSpecifier3);

    unit_.value().AddServiceInstanceDeployments(kInstanceSpecifier1, deployment1);
    unit_.value().AddServiceInstanceDeployments(kInstanceSpecifier2, deployment2);
    unit_.value().AddServiceInstanceDeployments(kInstanceSpecifier3, deployment3);

    // When calling GetInstanceOfServiceType with identifier "foo"
    const auto result = unit_.value().GetInstancesOfServiceType("foo");

    // Then we should find 2 entries with the respective instance specifiers
    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE(result.find("abc/abc/TirePressurePort1") != result.end());
    EXPECT_TRUE(result.find("abc/abc/TirePressurePort3") != result.end());
}

TEST_F(ConfigurationFixture, MergingTwoConfigurationsWithUniqueServiceIdentifierTypesAndInstanceSpecifiersSucceeds)
{
    // Given a configuration with at lest one entry...
    WithMinimalConfiguration();

    LolaServiceId service_id{1U};
    auto instance_specifier_string = InstanceSpecifier::Create(std::string{"/bla/blob/instance_specifier"}).value();
    ConfigurationStore config_store{
        instance_specifier_string,
        make_ServiceIdentifierType("/bla/blob/one", 1U, 2U),
        QualityType::kASIL_QM,
        service_id,
        LolaServiceInstanceId{1U},
    };

    // ... and a second configuration that has an identical service instance entry
    Configuration::ServiceTypeDeployments type_deployments{};
    type_deployments.insert({config_store.service_identifier_, *config_store.service_type_deployment_});
    Configuration::ServiceInstanceDeployments instance_deployments{};
    instance_deployments.emplace(config_store.instance_specifier_, *config_store.service_instance_deployment_);

    auto addon_configuration =
        Configuration{type_deployments, instance_deployments, GlobalConfiguration{}, TracingConfiguration{}};

    // When merging the two configurations
    const auto merge_result = unit_.value().MergeServiceEntries(std::move(addon_configuration));

    // Then the error code should be the expected one
    EXPECT_TRUE(merge_result.has_value());
    EXPECT_EQ(unit_.value().GetNumberOfServiceTypes(), 2);
    EXPECT_EQ(unit_.value().GetNumberOfServiceInstances(), 2);
}

TEST_F(ConfigurationFixture, MergingIntoEmptyConfigurationLeadsToResultingConfigEqualsIncomingConfig)
{
    // Given an empty configuration ...
    WithEmptyConfiguration();

    // ... and an add-on configuration with some entries
    Configuration::ServiceTypeDeployments type_deployments{};
    type_deployments.insert({kConfigStoreQm.service_identifier_, *kConfigStoreQm.service_type_deployment_});
    Configuration::ServiceInstanceDeployments instance_deployments{};
    instance_deployments.emplace(kConfigStoreQm.instance_specifier_, *kConfigStoreQm.service_instance_deployment_);

    auto addon_config =
        Configuration{type_deployments, instance_deployments, GlobalConfiguration{}, TracingConfiguration{}};

    // When merging both configurations
    const auto merge_result = unit_.value().MergeServiceEntries(std::move(addon_config));

    // Then merging should be successful and the resulting config should have the same entries as the add-on
    // configuration
    EXPECT_TRUE(merge_result.has_value());

    EXPECT_EQ(GetServiceTypesSnapshot(unit_.value()), type_deployments);
    EXPECT_EQ(GetServiceInstancesSnapshot(unit_.value()), instance_deployments);
}

TEST_F(ConfigurationFixture, MergingEmptyConfigurationLeadsToResultingConfigEqualsInitialConfig)
{
    // Given a configuration with some entries
    WithMinimalConfiguration();

    const auto type_deployments_backup = GetServiceTypesSnapshot(unit_.value());
    const auto instance_deployments_backup = GetServiceInstancesSnapshot(unit_.value());

    // ... and an empty configuration that shall be merged
    auto addon_configuration = Configuration{Configuration::ServiceTypeDeployments{},
                                             Configuration::ServiceInstanceDeployments{},
                                             GlobalConfiguration{},
                                             TracingConfiguration{}};

    // When merging these two configuration
    const auto merge_result = unit_.value().MergeServiceEntries(std::move(addon_configuration));

    // Then merging should be successful and the resulting config should have the same entries as the initial
    // configuration
    EXPECT_TRUE(merge_result.has_value());

    EXPECT_EQ(GetServiceTypesSnapshot(unit_.value()), type_deployments_backup);
    EXPECT_EQ(GetServiceInstancesSnapshot(unit_.value()), instance_deployments_backup);
}

TEST_F(ConfigurationFixture, MergingWithDuplicateServiceTypeEntriesLeadsToError)
{
    // Given a configuration with at lest one entry...
    WithMinimalConfiguration();

    // ... and a second configuration that has an identical service type entry
    Configuration::ServiceTypeDeployments type_deployments{};
    type_deployments.insert({kConfigStoreQm.service_identifier_, *kConfigStoreQm.service_type_deployment_});
    Configuration::ServiceInstanceDeployments instance_deployments{};
    instance_deployments.emplace(kConfigStoreQm.instance_specifier_, *kConfigStoreQm.service_instance_deployment_);

    auto addon_configuration =
        Configuration{type_deployments, instance_deployments, GlobalConfiguration{}, TracingConfiguration{}};

    // When merging the two configurations
    const auto merge_result = unit_.value().MergeServiceEntries(std::move(addon_configuration));

    // Then the error code should be the expected one
    EXPECT_FALSE(merge_result.has_value());
    EXPECT_EQ(merge_result.error(), configuration_errc::configuration_merge_duplicate_service_type);
}

TEST_F(ConfigurationFixture, MergingWithDuplicateServiceInstanceEntriesLeadsToError)
{
    // Given a configuration with at lest one entry...
    WithMinimalConfiguration();

    LolaServiceId service_id{1U};
    auto instance_specifier_string = InstanceSpecifier::Create(std::string{"/bla/blob/instance_specifier"}).value();
    ConfigurationStore config_store{
        instance_specifier_string,
        make_ServiceIdentifierType("/bla/blob/one", 1U, 2U),
        QualityType::kASIL_QM,
        service_id,
        LolaServiceInstanceId{1U},
    };

    // ... and a second configuration that has an identical service instance entry
    Configuration::ServiceTypeDeployments type_deployments{};
    type_deployments.insert({config_store.service_identifier_, *config_store.service_type_deployment_});
    Configuration::ServiceInstanceDeployments instance_deployments{};
    instance_deployments.emplace(kConfigStoreQm.instance_specifier_, *kConfigStoreQm.service_instance_deployment_);

    auto addon_configuration =
        Configuration{type_deployments, instance_deployments, GlobalConfiguration{}, TracingConfiguration{}};

    // When merging the two configurations
    const auto merge_result = unit_.value().MergeServiceEntries(std::move(addon_configuration));

    // Then the error code should be the expected one
    EXPECT_FALSE(merge_result.has_value());
    EXPECT_EQ(merge_result.error(), configuration_errc::configuration_merge_duplicate_service_instance);
}

TEST_F(ConfigurationFixture, MergingConfigurationsFromTwoThreadsConcurrentlySucceeds)
{
    // Given an empty configuration ...
    WithEmptyConfiguration();

    constexpr std::size_t kMergesPerThread{50U};

    // ... and a helper which merges kMergesPerThread uniquely-named addon configurations (identified via
    // thread_index/entry_index) into unit_ and records whether each merge succeeded.
    auto merge_worker = [this](std::size_t thread_index) {
        std::vector<bool> results{};
        results.reserve(kMergesPerThread);
        for (std::size_t entry_index = 0U; entry_index < kMergesPerThread; ++entry_index)
        {
            const auto service_name =
                "/thread" + std::to_string(thread_index) + "/service" + std::to_string(entry_index);
            const auto instance_specifier_string = InstanceSpecifier::Create("/thread" + std::to_string(thread_index) +
                                                                             "/instance" + std::to_string(entry_index))
                                                       .value();

            Configuration::ServiceTypeDeployments type_deployments{};
            type_deployments.insert(
                {make_ServiceIdentifierType(service_name, 1U, 0U), *kConfigStoreQm.service_type_deployment_});
            Configuration::ServiceInstanceDeployments instance_deployments{};
            instance_deployments.emplace(instance_specifier_string, *kConfigStoreQm.service_instance_deployment_);

            Configuration addon_configuration{std::move(type_deployments),
                                              std::move(instance_deployments),
                                              GlobalConfiguration{},
                                              TracingConfiguration{}};

            const auto merge_result = unit_.value().MergeServiceEntries(addon_configuration);
            results.push_back(merge_result.has_value());
        }
        return results;
    };

    // When merging configurations into the shared unit_ concurrently from two different threads, each with its own
    // uniquely-named set of service types/instances (so no clashes can occur between the threads)
    std::vector<bool> results_thread_0{};
    std::vector<bool> results_thread_1{};
    std::thread thread_0([&results_thread_0, &merge_worker]() {
        results_thread_0 = merge_worker(0U);
    });
    std::thread thread_1([&results_thread_1, &merge_worker]() {
        results_thread_1 = merge_worker(1U);
    });
    thread_0.join();
    thread_1.join();

    // Then every single merge should have succeeded ...
    EXPECT_EQ(results_thread_0.size(), kMergesPerThread);
    EXPECT_EQ(results_thread_1.size(), kMergesPerThread);
    EXPECT_TRUE(std::all_of(results_thread_0.begin(), results_thread_0.end(), [](bool ok) {
        return ok;
    }));
    EXPECT_TRUE(std::all_of(results_thread_1.begin(), results_thread_1.end(), [](bool ok) {
        return ok;
    }));

    // ... and the resulting configuration should contain all entries from both threads, without any lost updates.
    EXPECT_EQ(unit_.value().GetNumberOfServiceTypes(), 2U * kMergesPerThread);
    EXPECT_EQ(unit_.value().GetNumberOfServiceInstances(), 2U * kMergesPerThread);
}

TEST_F(ConfigurationFixture, MergingConfigurationDoesNotBreakExistingReferences)
{
    // Given a minimal configuration with at least one entry, ...
    WithMinimalConfiguration();
    // ...for which we can store a reference to
    const auto service_type_deployment =
        unit_.value().GetServiceTypeDeployment(make_ServiceIdentifierType("/bla/blub/one", 1U, 2U)).value().get();
    const auto service_instance_deployment =
        unit_.value()
            .GetServiceInstanceDeployment(
                InstanceSpecifier::Create(std::string{"/bla/blub/instance_specifier"}).value())
            .value()
            .get();

    // ... and a second configuration with another valid entry
    LolaServiceId service_id{1U};
    auto instance_specifier_string = InstanceSpecifier::Create(std::string{"/bla/blob/instance_specifier"}).value();
    ConfigurationStore config_store{
        instance_specifier_string,
        make_ServiceIdentifierType("/bla/blob/one", 1U, 2U),
        QualityType::kASIL_QM,
        service_id,
        LolaServiceInstanceId{1U},
    };

    Configuration::ServiceTypeDeployments type_deployments{};
    type_deployments.insert({config_store.service_identifier_, *config_store.service_type_deployment_});
    Configuration::ServiceInstanceDeployments instance_deployments{};
    instance_deployments.emplace(config_store.instance_specifier_, *config_store.service_instance_deployment_);

    auto addon_configuration =
        Configuration{type_deployments, instance_deployments, GlobalConfiguration{}, TracingConfiguration{}};

    // When merging the two configurations
    const auto merge_result = unit_.value().MergeServiceEntries(std::move(addon_configuration));

    const auto lola_service_type_deployment =
        std::get<LolaServiceTypeDeployment>(service_type_deployment.binding_info_);

    // Then the merge should be successful, and we should still be able to access the previously given reference
    EXPECT_TRUE(merge_result.has_value());
    EXPECT_EQ(lola_service_type_deployment.service_id_, 1);
    EXPECT_EQ(service_instance_deployment.instance_specifier_.ToString(), "/bla/blub/instance_specifier");
}

TEST_F(ConfigurationFixture, MergingIntoConfigurationWithInvalidStateReturnsError)
{
    // Given a configuration with at least one entry, that has then been moved-from (leaving its internal service
    // type / instance deployment pointers reset to null)...
    WithMinimalConfiguration();
    const Configuration configuration_moved_to{std::move(unit_.value())};

    // Sanity check: the object moved into should be unaffected and still contain the original entries
    EXPECT_EQ(configuration_moved_to.GetNumberOfServiceTypes(), 1);
    EXPECT_EQ(configuration_moved_to.GetNumberOfServiceInstances(), 1);

    // ... and a valid add-on configuration to merge in
    Configuration::ServiceTypeDeployments type_deployments{};
    type_deployments.insert({kConfigStoreQm.service_identifier_, *kConfigStoreQm.service_type_deployment_});
    Configuration::ServiceInstanceDeployments instance_deployments{};
    instance_deployments.emplace(kConfigStoreQm.instance_specifier_, *kConfigStoreQm.service_instance_deployment_);

    auto addon_configuration =
        Configuration{type_deployments, instance_deployments, GlobalConfiguration{}, TracingConfiguration{}};

    // When attempting to merge the add-on configuration into the now invalid configuration
    const auto merge_result = unit_.value().MergeServiceEntries(std::move(addon_configuration));

    // Then the merge should fail with the expected error
    EXPECT_FALSE(merge_result.has_value());
    EXPECT_EQ(merge_result.error(), configuration_errc::configuration_merged_invalid_configuration_state);
}

using ConfigurationDeathTest = ConfigurationFixture;
TEST_F(ConfigurationDeathTest, AddingAServiceTypeDeploymentWithDuplicateServiceIdentifierTypeTerminates)
{
    // Given a configuration which contains a ServiceTypeDeployment corresponding to a ServiceIdentifierType
    WithMinimalConfiguration();

    // When inserting another ServiceTypeDeployment with the same ServiceIdentifierType
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit_.value().AddServiceTypeDeployment(kConfigStoreQm.service_identifier_,
                                                                             *kConfigStoreQm.service_type_deployment_),
                 ".*");
}

TEST_F(ConfigurationDeathTest, AddingAServiceInstanceDeploymentWithDuplicateInstanceSpecifierTerminates)
{
    // Given a configuration which contains a ServiceInstanceDeployment corresponding to a InstanceSpecifier
    WithMinimalConfiguration();

    // When inserting another ServiceInstanceDeployment with the same InstanceSpecifier
    // Then the program terminates
    EXPECT_DEATH(score::cpp::ignore = unit_.value().AddServiceInstanceDeployments(
                     kConfigStoreQm.instance_specifier_, *kConfigStoreQm.service_instance_deployment_),
                 ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
