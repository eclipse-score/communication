/*******************************************************************************
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
 *******************************************************************************/

#include "score/mw/com/test/check_values_created_from_config/check_values_created_from_config_application.h"

#include "score/os/utils/interprocess/interprocess_notification.h"
#include "score/result/result.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/config_parser.h"
#include "score/mw/com/impl/configuration/lola_event_id.h"
#include "score/mw/com/impl/configuration/lola_service_instance_id.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/service_element_type.h"
#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"

#include "score/os/errno.h"
#include "score/mw/log/logging.h"

#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace
{

using InstanceSpecifier = score::mw::com::impl::InstanceSpecifier;
using ElementFqId = score::mw::com::impl::lola::ElementFqId;

using std::string_view_literals::operator""sv;

#if defined(__QNXNTO__)
constexpr auto kSharedMemoryPathPrefix = "/dev/shmem/";
#else
constexpr auto kSharedMemoryPathPrefix = "/dev/shm/";
#endif

class ConfigParser
{
  public:
    ConfigParser(const std::string& service_instance_manifest_path, const InstanceSpecifier instance_specifier)
    {
        const auto configuration = score::mw::com::impl::configuration::Parse(service_instance_manifest_path);

        const score::mw::com::impl::ServiceIdentifierType service_identifier_type =
            score::mw::com::impl::make_ServiceIdentifierType(
                std::string{service_type_name_}, major_version_number_, minor_version_number_);

        type_deployment_ = configuration.GetServiceTypes().at(service_identifier_type);

        const auto deployment = configuration.GetServiceInstances().at(instance_specifier);
        lola_instance_binding_ = std::get<score::mw::com::impl::LolaServiceInstanceDeployment>(deployment.bindingInfo_);
    }

    score::Result<ElementFqId> GetElementFQId(const score::mw::com::impl::LolaEventId event_id) const noexcept
    {
        const auto instance_id = lola_instance_binding_.instance_id_.value().GetId();
        const auto lola_event_id = static_cast<std::uint8_t>(event_id);

        const auto* const lola_service_type_deployment =
            std::get_if<score::mw::com::impl::LolaServiceTypeDeployment>(&type_deployment_.binding_info_);

        if (lola_service_type_deployment == nullptr)
        {
            return score::MakeUnexpected(score::mw::com::impl::ComErrc::kInvalidBindingInformation,
                                       "No lola type deployment available.");
        }

        return ElementFqId{lola_service_type_deployment->service_id_,
                           lola_event_id,
                           instance_id,
                           score::mw::com::impl::ServiceElementType::EVENT};
    }

    score::cpp::optional<std::string> GetShmPath() const noexcept
    {
        const auto shm_name = GetShmName();
        if (!(shm_name.has_value()))
        {
            return {};
        }
        const auto shm_path = kSharedMemoryPathPrefix + shm_name.value();
        return shm_path;
    }

    score::cpp::optional<std::string> GetShmName() const noexcept
    {
        const auto lola_service_type_deployment =
            std::get_if<score::mw::com::impl::LolaServiceTypeDeployment>(&type_deployment_.binding_info_);
        if (lola_service_type_deployment == nullptr)
        {
            return {};
        }

        score::mw::com::impl::lola::ShmPathBuilder shm_path_builder(lola_service_type_deployment->service_id_);
        return shm_path_builder.GetDataChannelShmName(lola_instance_binding_.instance_id_.value().GetId());
    }

  private:
    static constexpr auto service_type_name_ = "/score/adp/MapApiLanesStamped"sv;
    static constexpr std::uint32_t major_version_number_{1};
    static constexpr std::uint32_t minor_version_number_{0};

    score::mw::com::impl::LolaServiceInstanceDeployment lola_instance_binding_{};
    score::mw::com::impl::ServiceTypeDeployment type_deployment_{score::cpp::blank{}};
};

const std::string kInterprocessNotificationShmPath{"/lock"};

}  // namespace

/**
 * Integration test to test code requirements:
 *  SCR-6221534: SharedMemoryResources creates shared memory file under correct name.
 *  SCR-6285649: The Shared Memory Ressource shall find the underlying shared memory file under the correct name,
 *               derived from the InstanceIdentifier.
 *  SCR-6240632: ElementFqId shall be constructed from the associated configuration values.
 *
 * The test manually generates the shared memory file path, which should be created by the skeleton and opened by the
 * proxy, and the ElementFqIds of the events by parsing the configuration file. It then compares these values with those
 * used in the tests and ensures that they match.
 *
 * Since this test runs the proxy and skeleton in separate processes, it uses an InterprocessNotification object in
 * shared memory to synchronize the two processes.
 */
int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{Parameters::SERVICE_INSTANCE_MANIFEST, Parameters::MODE};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto stop_token = test_runner.GetStopToken();
    const auto stop_source = test_runner.GetStopSource();
    const auto service_instance_manifest_path = run_parameters.GetServiceInstanceManifest();
    const auto mode = run_parameters.GetMode();

    score::mw::com::test::EventSenderReceiver event_sender_receiver;

    const auto instance_specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/MapApiLanesStamped"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;

        return EXIT_FAILURE;
    }
    const auto& instance_specifier = instance_specifier_result.value();

    ConfigParser config_parser(service_instance_manifest_path, instance_specifier);

    const score::mw::com::impl::LolaEventId map_api_event_id{1};
    const auto map_api_lanes_element_fq_id = config_parser.GetElementFQId(map_api_event_id);
    if (!map_api_lanes_element_fq_id.has_value())
    {
        std::cout << "Could not get map_api_lanes_stamped ElementFqId from configuration. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    const score::mw::com::impl::LolaEventId dummy_event_id{2};
    const auto dummy_element_fq_id = config_parser.GetElementFQId(dummy_event_id);
    if (!dummy_element_fq_id.has_value())
    {
        std::cout << "Could not get dummy_data_stamped ElementFqId from configuration. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    const auto shared_memory_path = config_parser.GetShmPath();
    const auto shared_memory_name = config_parser.GetShmName();
    if (!shared_memory_path.has_value() || !shared_memory_name.has_value())
    {
        std::cout << "Could not get shared memory path/name from configuration. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    if (mode == "send" || mode == "skeleton")
    {
        std::cout << "Creating interprocess notification ...";
        auto interprocess_notification_result =
            score::mw::com::test::SharedMemoryObjectCreator<score::os::InterprocessNotification>::CreateObject(
                kInterprocessNotificationShmPath);
        if (!interprocess_notification_result.has_value())
        {
            std::stringstream ss;
            ss << "Creating interprocess notification object on skeleton side failed:"
               << interprocess_notification_result.error().ToString();
            std::cout << ss.str() << std::endl;
            return EXIT_FAILURE;
        }

        const score::mw::com::test::SharedMemoryObjectGuard<score::os::InterprocessNotification>
            interprocess_notification_guard{interprocess_notification_result.value()};
        return event_sender_receiver.RunAsSkeletonCheckValuesCreatedFromConfig(
            instance_specifier,
            shared_memory_path.value(),
            interprocess_notification_result.value().GetObject(),
            stop_source);
    }
    else if (mode == "recv" || mode == "proxy")
    {
        auto interprocess_notification_result =
            score::mw::com::test::SharedMemoryObjectCreator<score::os::InterprocessNotification>::CreateOrOpenObject(
                kInterprocessNotificationShmPath);
        if (!interprocess_notification_result.has_value())
        {
            std::stringstream ss;
            ss << "Creating or opening interprocess notification object on proxy side failed:"
               << interprocess_notification_result.error().ToString();
            std::cout << ss.str() << std::endl;
            return EXIT_FAILURE;
        }

        const score::mw::com::test::SharedMemoryObjectGuard<score::os::InterprocessNotification>
            interprocess_notification_guard{interprocess_notification_result.value()};
        return event_sender_receiver.RunAsProxyCheckValuesCreatedFromConfig(
            instance_specifier,
            map_api_lanes_element_fq_id.value(),
            dummy_element_fq_id.value(),
            shared_memory_name.value(),
            interprocess_notification_result.value().GetObject(),
            stop_token);
    }
    else
    {
        std::cerr << "Unknown mode " << mode << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }
}
