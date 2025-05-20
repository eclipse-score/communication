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

#include "score/mw/com/test/shared_memory_storage/shared_memory_storage_application.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"
#include "score/mw/com/test/shared_memory_storage/test_resources.h"

#include "score/os/utils/interprocess/interprocess_notification.h"

#include <stdint.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <ostream>
#include <string>

namespace
{

    using BigDataServiceElementData = bmw::mw::com::test::BigDataServiceElementData;
    using MapApiLanesStamped = bmw::mw::com::test::MapApiLanesStamped;
    using DummyDataStamped = bmw::mw::com::test::DummyDataStamped;

    const std::string kInterprocessElementAddressesShmPath{"/service_data_storage_element_addresses"};
    const std::string kProxyDoneInterprocessNotifierShmPath{"/proxy_done_interprocess_notifier_creator"};
    const std::string kSkeletonDoneInterprocessNotifierShmPath{"/skeleton_done_interprocess_notifier_creator"};

    template <typename ServiceElementViewType, typename LolaServiceElementType, typename ServiceElementType>
    amp::optional<bmw::mw::com::impl::lola::ElementFqId> GetElementFqId(ServiceElementType &service_element)
    {
        auto *const map_api_lanes_stamped_binding =
            bmw::mw::com::test::GetLolaBinding<ServiceElementViewType, LolaServiceElementType>(service_element);
        if (map_api_lanes_stamped_binding == nullptr)
        {
            return {};
        }
        return map_api_lanes_stamped_binding->GetElementFQId();
    }

    template <typename ServiceType, typename ServiceViewType, typename LolaServiceType, typename LolaServiceAttorneyType>
    amp::optional<std::array<uintptr_t, 2>> GetTypeMetaInfoAddresses(
        ServiceType &service,
        const std::array<bmw::mw::com::impl::lola::ElementFqId, 2> &element_fq_ids)
    {
        auto *const binding = bmw::mw::com::test::GetLolaBinding<ServiceViewType, LolaServiceType>(service);
        if (binding == nullptr)
        {
            std::cerr << "Could not get lola binding.\n";
            return {};
        }
        const LolaServiceAttorneyType attorney{*binding};
        const auto map_api_lanes_type_meta_information_address_result = attorney.GetEventMetaInfoAddress(element_fq_ids[1]);
        if (!(map_api_lanes_type_meta_information_address_result.has_value()))
        {
            std::cerr << "Could not get map_api_lanes event meta info.\n";
            return {};
        }
        const auto dummy_data_type_meta_information_address_result = attorney.GetEventMetaInfoAddress(element_fq_ids[0]);
        if (!(dummy_data_type_meta_information_address_result.has_value()))
        {
            std::cerr << "Could not get dummy_data event meta info.\n";
            return {};
        }

        return {{map_api_lanes_type_meta_information_address_result.value(),
                 dummy_data_type_meta_information_address_result.value()}};
    }

} // namespace

/// \brief ITF / SCTF test which checks that the addresses of EventMetaInfo objects and the ElementFqId stored in
/// service elements are the same in Skeleton and Proxy service elements.
///
/// ITF / SCTF test which satisfies requirements:
/// - 32391820: Checks that the ElementFqId used to identify a service element is the exact same on the Proxy and
///             Skeleton side.
/// - 32391820: Checks that the storage location of the type meta information of a service element is stored at the same
///             address (relative to the start of the memory region in each process)
int main(int argc, const char **argv)
{
    using Parameters = bmw::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{Parameters::MODE, Parameters::NUM_CYCLES, Parameters::CYCLE_TIME};
    bmw::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto &run_parameters = test_runner.GetRunParameters();
    const auto mode = run_parameters.GetMode();
    const auto stop_token = test_runner.GetStopToken();

    auto skeleton_side_service_element_data_shm_creator_result =
        bmw::mw::com::test::SharedMemoryObjectCreator<BigDataServiceElementData>::CreateOrOpenObject(
            kInterprocessElementAddressesShmPath);
    if (!skeleton_side_service_element_data_shm_creator_result.has_value())
    {
        std::stringstream ss;
        ss << "Creating or opening interprocess notification object failed:"
           << skeleton_side_service_element_data_shm_creator_result.error().ToString();
        std::cout << ss.str() << std::endl;
        return EXIT_FAILURE;
    }
    auto &skeleton_side_service_element_data_shm_creator =
        skeleton_side_service_element_data_shm_creator_result.value();
    const bmw::mw::com::test::SharedMemoryObjectGuard<BigDataServiceElementData>
        skeleton_side_service_element_data_shm_guard{skeleton_side_service_element_data_shm_creator};

    auto proxy_done_interprocess_notifier_creator_result =
        bmw::mw::com::test::SharedMemoryObjectCreator<bmw::os::InterprocessNotification>::CreateOrOpenObject(
            kProxyDoneInterprocessNotifierShmPath);
    if (!proxy_done_interprocess_notifier_creator_result.has_value())
    {
        std::stringstream ss;
        ss << "Creating or opening interprocess notification object for proxy done failed:"
           << proxy_done_interprocess_notifier_creator_result.error().ToString();
        std::cout << ss.str() << std::endl;
        return EXIT_FAILURE;
    }
    auto &proxy_done_interprocess_notifier_creator = proxy_done_interprocess_notifier_creator_result.value();
    const bmw::mw::com::test::SharedMemoryObjectGuard<bmw::os::InterprocessNotification>
        proxy_done_interprocess_notifier_guard{proxy_done_interprocess_notifier_creator};

    auto skeleton_done_interprocess_notifier_creator_result =
        bmw::mw::com::test::SharedMemoryObjectCreator<bmw::os::InterprocessNotification>::CreateOrOpenObject(
            kSkeletonDoneInterprocessNotifierShmPath);
    if (!skeleton_done_interprocess_notifier_creator_result.has_value())
    {
        std::stringstream ss;
        ss << "Creating or opening interprocess notification object for service offered failed:"
           << skeleton_done_interprocess_notifier_creator_result.error().ToString();
        std::cout << ss.str() << std::endl;
        return EXIT_FAILURE;
    }
    auto &skeleton_done_interprocess_notifier_creator = skeleton_done_interprocess_notifier_creator_result.value();
    const bmw::mw::com::test::SharedMemoryObjectGuard<bmw::os::InterprocessNotification>
        skeleton_done_interprocess_notifier_guard{skeleton_done_interprocess_notifier_creator};

    auto instance_specifier = bmw::mw::com::InstanceSpecifier::Create("xpad/cp60/MapApiLanesStamped").value();
    if (mode == "send" || mode == "skeleton")
    {
        std::cout << "Skeleton: Running as skeleton\n";

        // Create NotifierGuard so that the skeleton_done notifier will be notified when it goes out of scope. This will
        // occur when we exit either at the end of the test or if we return early with a failure.
        bmw::mw::com::test::NotifierGuard skeleton_done_notifier_guard{
            skeleton_done_interprocess_notifier_creator.GetObject()};

        // ********************************************************************************
        // Create Skeleton
        // ********************************************************************************
        auto bigdata_result = bmw::mw::com::test::BigDataSkeleton::Create(instance_specifier);
        if (!bigdata_result.has_value())
        {
            std::cerr << "Skeleton: Unable to construct BigDataSkeleton: " << bigdata_result.error() << ", bailing!\n";
            return EXIT_FAILURE;
        }
        auto &bigdata_skeleton = bigdata_result.value();

        // ********************************************************************************
        // Offer Skeleton
        // ********************************************************************************
        const auto offer_result = bigdata_skeleton.OfferService();
        if (!offer_result.has_value())
        {
            std::cerr << "Skeleton: Unable to offer service for BigDataSkeleton: " << offer_result.error()
                      << ", bailing!\n";
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Get ElementFqId of SkeletonEvents
        // ********************************************************************************
        const auto map_api_lanes_element_fq_id_result =
            GetElementFqId<bmw::mw::com::impl::SkeletonEventView<MapApiLanesStamped>,
                           bmw::mw::com::impl::lola::SkeletonEvent<MapApiLanesStamped>>(
                bigdata_skeleton.map_api_lanes_stamped_);
        if (!(map_api_lanes_element_fq_id_result.has_value()))
        {
            std::cerr << "Skeleton: Could not get map_api_lanes ElementFqId, bailing\n";
            return EXIT_FAILURE;
        }
        const auto dummy_data_element_fq_id_result =
            GetElementFqId<bmw::mw::com::impl::SkeletonEventView<DummyDataStamped>,
                           bmw::mw::com::impl::lola::SkeletonEvent<DummyDataStamped>>(
                bigdata_skeleton.dummy_data_stamped_);
        if (!(map_api_lanes_element_fq_id_result.has_value()))
        {
            std::cerr << "Skeleton: Could not get dummy_data ElementFqId, bailing\n";
            return EXIT_FAILURE;
        }
        const std::array<bmw::mw::com::impl::lola::ElementFqId, 2> element_fq_ids = {
            map_api_lanes_element_fq_id_result.value(), dummy_data_element_fq_id_result.value()};

        // ********************************************************************************
        // Get address of meta information for SkeletonEvents
        // ********************************************************************************
        const auto event_meta_info_addresseses =
            GetTypeMetaInfoAddresses<bmw::mw::com::test::BigDataSkeleton,
                                     bmw::mw::com::impl::SkeletonBaseView,
                                     bmw::mw::com::impl::lola::Skeleton,
                                     bmw::mw::com::impl::lola::SkeletonAttorney>(bigdata_skeleton, element_fq_ids);
        if (!(event_meta_info_addresseses.has_value()))
        {
            std::cerr << "Proxy: Could not get event meta info addresses\n";
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Store addresses in interprocess object
        // ********************************************************************************
        const BigDataServiceElementData skeleton_side_service_element_data{element_fq_ids,
                                                                           event_meta_info_addresseses.value()};
        skeleton_side_service_element_data_shm_creator.GetObject() = skeleton_side_service_element_data;

        // ********************************************************************************
        // Notify proxy side that the skeleton has finished writing service element data to shared memory via
        // interprocess object
        // ********************************************************************************
        skeleton_done_interprocess_notifier_creator.GetObject().notify();

        // ********************************************************************************
        // Wait on interprocess notifier in shared memory
        // ********************************************************************************
        if (!proxy_done_interprocess_notifier_creator.GetObject().waitWithAbort(stop_token))
        {
            std::cerr << "Abort received while waiting for proxy done notifier\n";
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (mode == "recv" || mode == "proxy")
    {
        std::cout << "Proxy: Running as proxy\n";

        // Create NotifierGuard so that the proxy_done notifier will be notified when it goes out of scope. This will
        // occur when we exit either at the end of the test or if we return early with a failure.
        bmw::mw::com::test::NotifierGuard proxy_done_notifier_guard{
            proxy_done_interprocess_notifier_creator.GetObject()};

        // ********************************************************************************
        // StartFindService -> Create Proxy
        // ********************************************************************************
        bmw::mw::com::test::ProxyCreationData proxy_creation_data{};
        auto find_service_callback = [&proxy_creation_data](auto service_handle_container,
                                                            auto find_service_handle) mutable noexcept
        {
            std::cout << "Proxy: find service handler called" << std::endl;
            std::lock_guard lock{proxy_creation_data.mutex};
            if (service_handle_container.size() != 1)
            {
                std::cerr << "Proxy: service handle container should contain 1 handle but contains: "
                          << service_handle_container.size() << std::endl;
                proxy_creation_data.condition_variable.notify_all();
                return;
            }
            proxy_creation_data.handle =
                std::make_unique<bmw::mw::com::test::BigDataProxy::HandleType>(service_handle_container[0]);
            bmw::mw::com::test::BigDataProxy::StopFindService(find_service_handle);
        };

        auto start_find_service_result =
            bmw::mw::com::test::BigDataProxy::StartFindService(find_service_callback, instance_specifier);
        if (!start_find_service_result.has_value())
        {
            std::cerr << "Proxy: StartFindService() failed:" << start_find_service_result.error().Message().data()
                      << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "Proxy: StartFindService called" << std::endl;

        // Wait for the find service handler to be called
        std::unique_lock proxy_creation_lock{proxy_creation_data.mutex};
        proxy_creation_data.condition_variable.wait_for(
            proxy_creation_lock, std::chrono::seconds{2U}, [&proxy_creation_data]
            { return proxy_creation_data.handle == nullptr; });
        auto proxy_result = bmw::mw::com::test::BigDataProxy::Create(*proxy_creation_data.handle);
        proxy_creation_lock.unlock();
        if (!proxy_result.has_value())
        {
            std::cerr << "Proxy: Unable to construct BigDataProxy: " << proxy_result.error() << ", bailing!\n";
            return EXIT_FAILURE;
        }
        auto &bigdata_proxy = proxy_result.value();
        std::cout << "Proxy: BigDataProxy created" << std::endl;

        // ********************************************************************************
        // Get ElementFqId of ProxyEvents
        // ********************************************************************************
        const auto map_api_lanes_element_fq_id_result =
            GetElementFqId<bmw::mw::com::impl::ProxyEventView<MapApiLanesStamped>,
                           bmw::mw::com::impl::lola::ProxyEvent<MapApiLanesStamped>>(
                bigdata_proxy.map_api_lanes_stamped_);
        if (!(map_api_lanes_element_fq_id_result.has_value()))
        {
            std::cerr << "Proxy: Could not get map_api_lanes ElementFqId, bailing\n";
            return EXIT_FAILURE;
        }
        const auto dummy_data_element_fq_id_result =
            GetElementFqId<bmw::mw::com::impl::ProxyEventView<DummyDataStamped>,
                           bmw::mw::com::impl::lola::ProxyEvent<DummyDataStamped>>(bigdata_proxy.dummy_data_stamped_);
        if (!(map_api_lanes_element_fq_id_result.has_value()))
        {
            std::cerr << "Proxy: Could not get dummy_data ElementFqId, bailing\n";
            return EXIT_FAILURE;
        }
        const std::array<bmw::mw::com::impl::lola::ElementFqId, 2> element_fq_ids = {
            map_api_lanes_element_fq_id_result.value(), dummy_data_element_fq_id_result.value()};

        // ********************************************************************************
        // Get address of meta information for an event from proxy
        // ********************************************************************************
        const auto event_meta_info_addresseses =
            GetTypeMetaInfoAddresses<bmw::mw::com::test::BigDataProxy,
                                     bmw::mw::com::impl::ProxyBaseView,
                                     bmw::mw::com::impl::lola::Proxy,
                                     bmw::mw::com::impl::lola::ProxyTestAttorney>(bigdata_proxy, element_fq_ids);
        if (!(event_meta_info_addresseses.has_value()))
        {
            std::cerr << "Proxy: Could not get event meta info addresses\n";
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Wait for the Skeleton to finish writing service element data to shared memory via interprocess object
        // ********************************************************************************
        if (!skeleton_done_interprocess_notifier_creator.GetObject().waitWithAbort(stop_token))
        {
            std::cerr << "Abort received while waiting for proxy done notifier\n";
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Get service element data from the Skeleton via interprocess object
        // ********************************************************************************
        const BigDataServiceElementData proxy_side_service_element_data{element_fq_ids,
                                                                        event_meta_info_addresseses.value()};

        // ********************************************************************************
        // Check that type event and meta information addresses are the same on skeleton
        // and proxy side
        // ********************************************************************************
        std::cout << "Comparing Skeleton side service element data \n("
                  << skeleton_side_service_element_data_shm_creator.GetObject()
                  << ") to proxy side service element data \n(" << proxy_side_service_element_data << ").\n";
        if (skeleton_side_service_element_data_shm_creator.GetObject() != proxy_side_service_element_data)
        {
            std::cerr << "Skeleton and proxy side service element data did not match\n.";
            return EXIT_FAILURE;
        }

        // ********************************************************************************
        // Notify skeleton side that the test is done and it can finish
        // ********************************************************************************
        proxy_done_interprocess_notifier_creator.GetObject().notify();

        return EXIT_SUCCESS;
    }

    std::cerr << "Invalid mode: " << mode << ", bailing!\n";
    return EXIT_FAILURE;
}
