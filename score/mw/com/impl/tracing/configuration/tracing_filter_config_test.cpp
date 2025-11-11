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
#include "score/mw/com/impl/tracing/configuration/tracing_filter_config.h"
#include "score/mw/com/impl/configuration/config_parser.h"

#include "score/mw/com/impl/tracing/configuration/proxy_event_trace_point_type.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace score::mw::com::impl::tracing
{
namespace
{

using score::json::operator""_json;
using std::string_view_literals::operator""sv;
const std::string kServiceType{"my_service_type"};
const std::string kEventName{"my_event_name"};
const std::string kFieldName{"my_field_name"};
const auto kInstanceSpecifiersv = "abc/abc/TirePressurePort"sv;
const ITracingFilterConfig::InstanceSpecifierView kInstanceSpecifierView{"my_instance_specifier"};
const score::cpp::optional<ITracingFilterConfig::InstanceSpecifierView> kEnableAllInstanceSpecifiers{};
constexpr auto kDummyTracePointType = SkeletonEventTracePointType::SEND;

template <typename TracePointTypeIn>
class TracingFilterConfigFixture : public ::testing::Test
{
};

// Gtest will run all tests in the TracingFilterConfigFixture once for every type, t, in MyTypes, such that TypeParam
// == t for each run.
using MyTypes = ::testing::
    Types<SkeletonEventTracePointType, SkeletonFieldTracePointType, ProxyEventTracePointType, ProxyFieldTracePointType>;
TYPED_TEST_SUITE(TracingFilterConfigFixture, MyTypes, );

TYPED_TEST(TracingFilterConfigFixture, CallingIsTracePointEnabledWithoutCallingAddReturnsFalse)
{
    const auto trace_point_type{static_cast<TypeParam>(1U)};

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    // When checking if a trace point with an instance id is enabled before adding it to the config
    const bool is_enabled =
        tracing_filter_config.IsTracePointEnabled(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type);

    // Then the trace point should be disabled
    EXPECT_FALSE(is_enabled);
}

TYPED_TEST(TracingFilterConfigFixture, CallingIsTracePointEnabledAfterCallingAddWithDifferentInstanceIdReturnsFalse)
{
    const ITracingFilterConfig::InstanceSpecifierView added_instance_specifier_view{"added_instance_specifier"};
    const ITracingFilterConfig::InstanceSpecifierView searched_instance_specifier_view{"searched_instance_specifier"};
    const auto trace_point_type{static_cast<TypeParam>(1U)};

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    // When adding a trace point with an instance id
    tracing_filter_config.AddTracePoint(kServiceType, kEventName, added_instance_specifier_view, trace_point_type);

    // and then when checking if a trace point with a different instance id is enabled
    const bool is_enabled = tracing_filter_config.IsTracePointEnabled(
        kServiceType, kEventName, searched_instance_specifier_view, trace_point_type);

    // Then the trace point should be disabled
    EXPECT_FALSE(is_enabled);
}

TYPED_TEST(TracingFilterConfigFixture, CallingIsTracePointEnabledAfterCallingAddReturnsTrue)
{
    const auto trace_point_type{static_cast<TypeParam>(1U)};

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    // When adding a trace point with an instance id
    tracing_filter_config.AddTracePoint(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type);

    // and then when checking if the trace point is enabled
    const bool is_enabled =
        tracing_filter_config.IsTracePointEnabled(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type);

    // Then the trace point should be enabled
    EXPECT_TRUE(is_enabled);
}

TYPED_TEST(TracingFilterConfigFixture, AddingSameTracePointTwiceWillNotCrash)
{
    const auto trace_point_type{static_cast<TypeParam>(1U)};

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    // When adding the same trace point with an instance id twice
    tracing_filter_config.AddTracePoint(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type);
    tracing_filter_config.AddTracePoint(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type);

    // Then we shouldn't crash

    // and then when checking if the trace point is enabled
    const bool is_enabled =
        tracing_filter_config.IsTracePointEnabled(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type);

    // Then the trace point should be enabled
    EXPECT_TRUE(is_enabled);
}

TEST(TracingFilterConfigTest, CheckingTracePointTypesWithSameNumericalValueDoNotMatch)
{
    const auto trace_point_type_0{static_cast<SkeletonEventTracePointType>(1U)};
    const auto trace_point_type_1{static_cast<ProxyEventTracePointType>(1U)};

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    // When adding a trace point with a trace point type
    tracing_filter_config.AddTracePoint(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type_0);

    // and then when checking if a trace point with the same identifiers and a service type with different enum type but
    // the same value
    const bool is_enabled =
        tracing_filter_config.IsTracePointEnabled(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type_1);

    // Then the trace point should be disabled
    EXPECT_FALSE(is_enabled);
}

TEST(TracingFilterConfigDeathTest, AddingInvalidTracePointTypeTerminates)
{
    const auto trace_point_type{SkeletonEventTracePointType::INVALID};

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    // When adding a trace point with an invalid trace point type it terminates
    EXPECT_DEATH(
        tracing_filter_config.AddTracePoint(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type), ".*");
}

static score::mw::com::impl::Configuration GetAraComConfigJson()
{
    std::string config_string_ = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": ")" +
                                 kServiceType +
                                 R"(",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": [
              {
                  "binding": "SHM",
                  "serviceId": 1234,
                  "events": [
                      {
                          "eventName": ")" +
                                 kEventName + R"(",
                          "eventId": 20
                      }
                  ],
                  "fields": [
                      {
                          "fieldName": ")" +
                                 kFieldName + R"(",
                          "fieldId": 21
                      }
                  ]
              }
          ]
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "my_instance_specifier",
            "serviceTypeName": ")" +
                                 kServiceType +
                                 R"(",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "instanceId": 1234,
                  "asil-level": "QM",
                  "binding": "SHM",
                  "events": [
                      {
                          "eventName": ")" +
                                 kEventName + R"(",
                          "numberOfSampleSlots": 50,
                          "maxSubscribers": 5,
                           "numberOfIpcTracingSlots": 3
                      }
                  ],
                  "fields": [
                      {
                          "fieldName": ")" +
                                 kFieldName + R"(",
                          "numberOfSampleSlots": 50,
                          "maxSubscribers": 5,
                           "numberOfIpcTracingSlots": 2
                      }
                  ]
                }
            ]
        }
    ]
  }
)";

    auto config_json = operator""_json(config_string_.data(), config_string_.size());

    return score::mw::com::impl::configuration::Parse(std::move(config_json));
}

TEST(AraComConfigJsonForTracingFilterConfigParserTestFixture,
     InsertingMultipleTracePointsFromSameServiceElementWithTraceDoneDoesNotCountMultiple)
{
    // Given an empty ipc tracing filter config, and a configuration with properly configured events and fields that
    // require tracing
    TracingFilterConfig tracing_filter_config{};
    auto config = GetAraComConfigJson();

    // When adding multiple trace points from the same service element, some of which require tracing with a Trace
    // done callback

    const auto trace_point_type_0 = SkeletonEventTracePointType::SEND;
    const auto trace_point_type_1 = SkeletonEventTracePointType::SEND_WITH_ALLOCATE;
    tracing_filter_config.AddTracePoint(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type_0);
    tracing_filter_config.AddTracePoint(kServiceType, kEventName, kInstanceSpecifierView, trace_point_type_1);

    const auto trace_point_type_2 = ProxyFieldTracePointType::GET_NEW_SAMPLES;
    const auto trace_point_type_3 = SkeletonFieldTracePointType::UPDATE;
    const auto trace_point_type_4 = SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE;
    tracing_filter_config.AddTracePoint(kServiceType, kFieldName, kInstanceSpecifierView, trace_point_type_2);
    tracing_filter_config.AddTracePoint(kServiceType, kFieldName, kInstanceSpecifierView, trace_point_type_3);
    tracing_filter_config.AddTracePoint(kServiceType, kFieldName, kInstanceSpecifierView, trace_point_type_4);

    // then the number of required tracing slots should be the same as the sum of the number of requested tracing slots,
    // of unique sevice elements, that are configured for tracing. In this case 3+2=5
    const auto number_of_required_tracing_slots = tracing_filter_config.GetNumberOfTracingSlots(config);
    EXPECT_EQ(number_of_required_tracing_slots, 5);
}

TEST(TracingFilterConfigGetNumberOfTraceingSlots, minimalTest)
{
    auto j2 = R"(
  {
    "serviceTypes": [
        {
          "serviceTypeName": "/bmw/ncar/services/TirePressureService",
          "version": {
              "major": 12,
              "minor": 34
          },
          "bindings": [
              {
                  "binding": "SHM",
                  "serviceId": 1234,
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "eventId": 20
                      }
                  ]
              }
          ]
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                  "instanceId": 1234,
                  "asil-level": "QM",
                  "binding": "SHM",
                  "events": [
                      {
                          "eventName": "CurrentPressureFrontLeft",
                          "maxSamples": 50,
                          "maxSubscribers": 5,
                           "numberOfIpcTracingSlots": 27
                      }
                  ],
                  "fields": []
                }
            ]
        }
    ]
  }
)"_json;
    // Given a config with an event asking for 27 tracing slots and a TracePointType, which requires
    // "TraceDoneCallback handling" and therefore requires sample slots for tracing
    auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto trace_point_type_0 = SkeletonEventTracePointType::SEND;

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    constexpr auto service_type = "/bmw/ncar/services/TirePressureService"sv;
    constexpr auto event_name = "CurrentPressureFrontLeft"sv;
    constexpr auto instance_specifier = "abc/abc/TirePressurePort"sv;

    // When adding a trace point for this TracePointType and the same service element, which has been configured with
    // the need for 27 sample slots for tracing
    tracing_filter_config.AddTracePoint(service_type, event_name, instance_specifier, trace_point_type_0);

    // then the overall number of tracing slots needed shall equal 27
    const auto number_of_tracing_slots = tracing_filter_config.GetNumberOfTracingSlots(config);
    EXPECT_EQ(number_of_tracing_slots, 27);
}

TEST(TracingFilterConfigGetNumberOfTraceingSlots, AFieldAlloneIsPresentAndWantsToBeTraced)
{
    auto j2 = R"(
{
    "serviceTypes": [
        {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "bindings": [
                {
                    "binding": "SHM",
                    "serviceId": 1234,
                    "events": [ ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "fieldId": 30
                        }
                    ]
                }
            ]
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                    "instanceId": 1234,
                    "asil-level": "QM",
                    "binding": "SHM",
                    "shm-size": 10000,
                    "events": [ ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": 7
                        }
                    ],
                    "allowedConsumer": {
                        "QM": [
                            42,
                            43
                        ],
                        "B": [
                            54,
                            55
                        ]
                    },
                    "allowedProvider": {
                        "QM": [
                            15
                        ],
                        "B": [
                            15
                        ]
                    }
                }
            ]
        }
    ]
}

)"_json;
    // Given a config with an event asking for 7 tracing slots
    auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto trace_point_type_0 = SkeletonFieldTracePointType::UPDATE;

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    constexpr std::string_view service_type = "/bmw/ncar/services/TirePressureService";
    constexpr std::string_view field_name = "CurrentTemperatureFrontLeft";
    constexpr std::string_view instance_specifier = "abc/abc/TirePressurePort";
    // When adding the trace points
    tracing_filter_config.AddTracePoint(service_type, field_name, instance_specifier, trace_point_type_0);

    // then the overall number of tracing slots needed shall equal 7
    const auto number_of_tracing_slots = tracing_filter_config.GetNumberOfTracingSlots(config);
    EXPECT_EQ(number_of_tracing_slots, 7);
}

TEST(TracingFilterConfigGetNumberOfTraceingSlots, AFieldAndAnEventArePresentAndWantsToBeTraced)
{
    auto j2 = R"(
    {
    "serviceTypes": [
        {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "bindings": [
                {
                    "binding": "SHM",
                    "serviceId": 1234,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "fieldId": 30
                        }
                    ]
                }
            ]
        }
    ],
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                    "instanceId": 1234,
                    "asil-level": "QM",
                    "binding": "SHM",
                    "shm-size": 10000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "numberOfSampleSlots": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 0
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": 7
                        }
                    ],
                    "allowedConsumer": {
                        "QM": [
                            42,
                            43
                        ],
                        "B": [
                            54,
                            55
                        ]
                    },
                    "allowedProvider": {
                        "QM": [
                            15
                        ],
                        "B": [
                            15
                        ]
                    }
                }
            ]
        }
    ]
}

)"_json;
    // Given a config with an event that is not traced and a field that is asking for 7 tracing slots
    // and TracePointTypes, which require "TraceDoneCallback handling" and therefore require sample slots for tracing
    auto config = score::mw::com::impl::configuration::Parse(std::move(j2));

    const auto trace_point_type_0 = SkeletonFieldTracePointType::UPDATE;
    const auto trace_point_type_1 = SkeletonEventTracePointType::SEND;

    // Given an empty ipc tracing filter config
    TracingFilterConfig tracing_filter_config{};

    constexpr std::string_view service_type = "/bmw/ncar/services/TirePressureService";
    constexpr std::string_view instance_specifier = "abc/abc/TirePressurePort";
    constexpr std::string_view field_name = "CurrentTemperatureFrontLeft";
    constexpr std::string_view event_name = "CurrentPressureFrontLeft";
    // When adding both trace points
    tracing_filter_config.AddTracePoint(service_type, field_name, instance_specifier, trace_point_type_0);
    tracing_filter_config.AddTracePoint(service_type, event_name, instance_specifier, trace_point_type_1);

    // then the overall number of tracing slots needed shall equal 7
    const auto number_of_tracing_slots = tracing_filter_config.GetNumberOfTracingSlots(config);
    EXPECT_EQ(number_of_tracing_slots, 7);
}

class ConfigurationFixture : public ::testing::Test
{
  public:
    void SetUp() override {}

    void TearDown() override {}

    using SampleSlotCountType = LolaEventInstanceDeployment::SampleSlotCountType;
    using TracingSlotSizeType = LolaEventInstanceDeployment::TracingSlotSizeType;

    using Fields = LolaServiceInstanceDeployment::FieldInstanceMapping;
    using Events = LolaServiceInstanceDeployment::EventInstanceMapping;

    template <typename Instance,
              typename = typename std::enable_if<std::is_same<Instance, LolaEventInstanceDeployment>::value ||
                                                 std::is_same<Instance, LolaFieldInstanceDeployment>::value>::type>
    Instance MakeLolaServiceInstanceDeployment(std::optional<SampleSlotCountType> number_of_sample_slots,
                                               const TracingSlotSizeType number_of_tracing_slots)
    {
        return Instance(number_of_sample_slots, 1U, 1U, false, number_of_tracing_slots);
    }

    InstanceSpecifier MakeInstanceSpecifier(std::string_view instance_specifier_sv)
    {
        auto instance_specifier_result = InstanceSpecifier::Create(std::string{instance_specifier_sv});
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(instance_specifier_result.has_value());
        return instance_specifier_result.value();
    }

    void PrepareValidConfigurationWithTracingRequiredEvent()
    {
        const auto valid_instance_specifier = MakeInstanceSpecifier(kInstanceSpecifiersv);
        auto events = Events{{event_name_, MakeLolaServiceInstanceDeployment<LolaEventInstanceDeployment>(1U, 1U)}};
        PrepareMinimalConfiguration(valid_instance_specifier, service_type_, events);
    }

    void PrepareMinimalConfiguration(InstanceSpecifier instance_specifier,
                                     std::string_view service_type_name,
                                     Events events = {},
                                     Fields fields = {})
    {
        auto service_type_deployment = ServiceTypeDeployment{score::cpp::blank{}};
        auto lola_instance_deployment = LolaServiceInstanceDeployment{LolaServiceInstanceId{1U}};

        lola_instance_deployment.fields_ = fields;
        lola_instance_deployment.events_ = events;

        auto service_identifier = make_ServiceIdentifierType(std::string{service_type_name}, 1U, 2U);

        InstanceSpecifier port_name{instance_specifier};

        auto service_instance_deployment = ServiceInstanceDeployment{
            service_identifier, lola_instance_deployment, QualityType::kASIL_QM, std::move(instance_specifier)};

        Configuration::ServiceTypeDeployments type_deployments{};
        type_deployments.insert({service_identifier, service_type_deployment});

        Configuration::ServiceInstanceDeployments instance_deployments{};
        instance_deployments.emplace(port_name, service_instance_deployment);

        configuration_.emplace(std::move(type_deployments),
                               std::move(instance_deployments),
                               GlobalConfiguration{},
                               TracingConfiguration{});
    }

    void PrepareConfigurationWithEventsDemandingTooManyTracingSlots()
    {

        Events events{};
        std::size_t event_count =
            std::numeric_limits<std::uint16_t>::max() / std::numeric_limits<std::uint8_t>::max() + 1;
        for (size_t i = 0; i < event_count; ++i)
        {
            const std::string event_name = "SomeEventName_" + std::to_string(i);
            constexpr std::uint8_t max_allowed_tracing_slots = std::numeric_limits<std::uint8_t>::max();
            const auto event =
                MakeLolaServiceInstanceDeployment<LolaEventInstanceDeployment>(1, max_allowed_tracing_slots);
            events.emplace(event_name, event);
        }

        const auto valid_instance_specifier = MakeInstanceSpecifier(kInstanceSpecifiersv);
        PrepareMinimalConfiguration(valid_instance_specifier, service_type_, events);

        for (const auto& [event_name, event] : events)
        {
            tracing_filter_config_.AddTracePoint(service_type_, event_name, kInstanceSpecifiersv, kDummyTracePointType);
        }
    }

    void PrepareAConfigurationThatDoesNotContainLolaDeployment()
    {
        auto service_type_deployment = ServiceTypeDeployment{score::cpp::blank{}};
        auto non_lola_deployment = score::cpp::blank{};

        auto service_identifier = make_ServiceIdentifierType(std::string{service_type_}, 1U, 2U);

        auto instance_specifier = MakeInstanceSpecifier(kInstanceSpecifiersv);

        InstanceSpecifier port_name{instance_specifier};
        auto service_instance_deployment = ServiceInstanceDeployment{
            service_identifier, non_lola_deployment, QualityType::kASIL_QM, std::move(instance_specifier)};

        Configuration::ServiceTypeDeployments type_deployments{};
        type_deployments.insert({service_identifier, service_type_deployment});

        Configuration::ServiceInstanceDeployments instance_deployments{};
        instance_deployments.emplace(port_name, service_instance_deployment);

        configuration_.emplace(std::move(type_deployments),
                               std::move(instance_deployments),
                               GlobalConfiguration{},
                               TracingConfiguration{});
    }
    score::cpp::optional<Configuration> configuration_{};
    TracingFilterConfig tracing_filter_config_{};
    const std::string_view service_type_ = "/bmw/ncar/services/TirePressureService";
    const std::string event_name_ = "CurrentPressureFrontLeft";
};

using TracingFilterConfigGetNumberOfTracingSlotsDeathTest = ConfigurationFixture;
TEST_F(TracingFilterConfigGetNumberOfTracingSlotsDeathTest, InstanceSpecifierCanNotBeParsed)
{

    // Given a valid config with an event that requires tracing and a trace point for this TracePointType and the
    // same service element, which contains an instance specifier whith illegal charachters.
    PrepareValidConfigurationWithTracingRequiredEvent();

    const std::string_view instance_specifier_sv = "specifier_with_bad_charachters%-&";
    tracing_filter_config_.AddTracePoint(service_type_, event_name_, instance_specifier_sv, kDummyTracePointType);

    // When calling GetNumberOfTracingSlots
    // Then The programm terminates
    EXPECT_DEATH(tracing_filter_config_.GetNumberOfTracingSlots(configuration_.value()), ".*");
}

TEST_F(TracingFilterConfigGetNumberOfTracingSlotsDeathTest, InstanceSpecifierCanNotBeFound)
{
    // Given a valid config with an event that requires tracing with a trace point for this TracePointType and the same
    // service element, Which contains a legal but wrong instance specifier.
    PrepareValidConfigurationWithTracingRequiredEvent();

    const std::string_view instance_specifier = "legal_but_wrong_instance_specifier";
    tracing_filter_config_.AddTracePoint(service_type_, event_name_, instance_specifier, kDummyTracePointType);

    // When calling GetNumberOfTracingSlots
    // Then we expect failiure
    EXPECT_DEATH(tracing_filter_config_.GetNumberOfTracingSlots(configuration_.value()), ".*");
}

TEST_F(TracingFilterConfigGetNumberOfTracingSlotsDeathTest,
       ProvidedServiceElementNameForTracingWhichDoesNotExistInConfig)
{
    // Given a valid config with an event that requires tracing
    // And a trace point for this TracePointType and the same service element, which does not exist in the config.
    PrepareValidConfigurationWithTracingRequiredEvent();

    const std::string_view wrong_event_name = "ThisServiceElementDoesNotExist";
    tracing_filter_config_.AddTracePoint(service_type_, wrong_event_name, kInstanceSpecifiersv, kDummyTracePointType);

    // When calling GetNumberOfTracingSlots
    // Then we expect failiure
    EXPECT_DEATH(tracing_filter_config_.GetNumberOfTracingSlots(configuration_.value()), ".*");
}

TEST_F(TracingFilterConfigGetNumberOfTracingSlotsDeathTest, RequestTooManyTracingSlots)
{

    // Given a valid config with an event that requires tracing, but requests one more than allowed total tracing slots
    PrepareConfigurationWithEventsDemandingTooManyTracingSlots();

    // When calling GetNumberOfTracingSlots,
    // Then we expect termination during the calculation of total number of tracing slots
    EXPECT_DEATH(tracing_filter_config_.GetNumberOfTracingSlots(configuration_.value()), ".*");
}

TEST_F(TracingFilterConfigGetNumberOfTracingSlotsDeathTest, ConfigurationContainsBlankDeployment)
{
    // Given a config that does not contain a ServiceInstanceDeployment but a legal trace point
    PrepareAConfigurationThatDoesNotContainLolaDeployment();

    tracing_filter_config_.AddTracePoint(service_type_, event_name_, kInstanceSpecifiersv, kDummyTracePointType);

    // When calling GetNumberOfTracingSlots
    // Then we expect failiure
    EXPECT_DEATH(tracing_filter_config_.GetNumberOfTracingSlots(configuration_.value()), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
