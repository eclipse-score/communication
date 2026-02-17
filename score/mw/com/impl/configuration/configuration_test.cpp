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
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include "score/json/internal/model/any.h"
#include "score/json/json_writer.h"

#include <score/optional.hpp>
#include <score/overload.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
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

    score::cpp::optional<Configuration> unit_{};
};

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
    EXPECT_EQ(unit2.GetServiceTypes().size(), 1);
    EXPECT_EQ(unit2.GetServiceInstances().size(), 1);
    EXPECT_NE(unit2.GetServiceInstances().find(kConfigStoreQm.instance_specifier_), unit2.GetServiceInstances().end());

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
    const auto* generated_lola_service_type =
        std::get_if<LolaServiceTypeDeployment>(&config.GetServiceTypes().at(service_identifier_type).binding_info_);
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
    const LolaFieldInstanceDeployment lola_field_instance{field_max_samples, field_max_subscribers, 1U, true, 7};
    const LolaMethodInstanceDeployment lola_method_instance{method_queue_size};

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
    const QualityType asil_level{QualityType::kASIL_B};

    ServiceInstanceDeployment manual_service_instance(
        service_identifier_type, binding_info, asil_level, instance_specifier_result.value());

    // Match ServiceInstances generated from json
    const ServiceInstanceDeployment& generated_service_instance =
        config.GetServiceInstances().at(instance_specifier_result.value());

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
