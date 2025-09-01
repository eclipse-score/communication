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
#include "sample_sender_receiver.h"
#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/handle_type.h"

#include "score/concurrency/notification.h"

#include "score/mw/com/impl/proxy_event.h"
#include <score/assert.hpp>
#include <score/hash.hpp>
#include <score/optional.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

using namespace std::chrono_literals;

namespace score::mw::com
{

namespace
{

constexpr std::size_t START_HASH = 64738U;

std::ostream& operator<<(std::ostream& stream, const InstanceSpecifier& instance_specifier)
{
    stream << instance_specifier.ToString();
    return stream;
}

template <typename T>
void ToStringImpl(std::ostream& o, T t)
{
    o << t;
}

template <typename T, typename... Args>
void ToStringImpl(std::ostream& o, T t, Args... args)
{
    ToStringImpl(o, t);
    ToStringImpl(o, args...);
}

template <typename... Args>
std::string ToString(Args... args)
{
    std::ostringstream oss;
    ToStringImpl(oss, args...);
    return oss.str();
}

void HashArray(const std::array<LaneIdType, 16U>& array, std::size_t& seed)
{
    const std::ptrdiff_t buffer_size =
        reinterpret_cast<const std::uint8_t*>(&*array.cend()) - reinterpret_cast<const std::uint8_t*>(&*array.cbegin());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(buffer_size > 0);
    seed = score::cpp::hash_bytes_fnv1a(static_cast<const void*>(array.data()), static_cast<std::size_t>(buffer_size), seed);
}

class SampleReceiver
{
  public:
    explicit SampleReceiver(const InstanceSpecifier& instance_specifier, bool check_sample_hash = true)
        : instance_specifier_{instance_specifier},
          last_received_{},
          received_{0U},
          check_sample_hash_{check_sample_hash}
    {
    }

    void ReceiveSample(const MapApiLanesStamped& map) noexcept
    {
        std::cout << ToString(instance_specifier_, ": Received sample: ", map.x, "\n");

        if (CheckReceivedSample(map))
        {
            received_ += 1U;
        }
        last_received_ = map.x;
    }

    std::size_t GetReceivedSampleCount() const noexcept
    {
        return received_;
    }

  private:
    bool CheckReceivedSample(const MapApiLanesStamped& map) const noexcept
    {
        if (last_received_.has_value())
        {
            if (map.x <= last_received_.value())
            {
                std::cerr << ToString(instance_specifier_,
                                      ": The received sample is out of order. Expected that ",
                                      map.x,
                                      " > ",
                                      last_received_.value(),
                                      "\n");
                return false;
            }
        }

        if (check_sample_hash_)
        {
            std::size_t hash_value = START_HASH;
            for (const MapApiLaneData& lane : map.lanes)
            {
                HashArray(lane.successor_lanes, hash_value);
            }

            if (hash_value != map.hash_value)
            {
                std::cerr << ToString(instance_specifier_,
                                      ": Unexpected data received, hash comparison failed: ",
                                      hash_value,
                                      ", expected ",
                                      map.hash_value,
                                      "\n");
                return false;
            }
        }

        return true;
    }

    const score::mw::com::InstanceSpecifier& instance_specifier_;
    score::cpp::optional<std::uint32_t> last_received_;
    std::size_t received_;
    bool check_sample_hash_;
};

score::cpp::optional<std::reference_wrapper<impl::ProxyEvent<MapApiLanesStamped>>> GetMapApiLanesStampedProxyEvent(
    IpcBridgeProxy& proxy)
{
    return proxy.map_api_lanes_stamped_;
}

score::cpp::optional<std::reference_wrapper<impl::GenericProxyEvent>> GetMapApiLanesStampedProxyEvent(
    GenericProxy& generic_proxy)
{
    const std::string event_name{"map_api_lanes_stamped"};
    auto event_it = generic_proxy.GetEvents().find(event_name);
    if (event_it == generic_proxy.GetEvents().cend())
    {
        std::cerr << "Could not find event " << event_name << " in generic proxy event map\n";
        return {};
    }
    return event_it->second;
}

/// \brief Function that returns the value pointed to by a pointer
const MapApiLanesStamped& GetSamplePtrValue(const MapApiLanesStamped* const sample_ptr)
{
    return *sample_ptr;
}

/// \brief Function that casts and returns the value pointed to by a void pointer
///
/// Assumes that the object in memory being pointed to is of type MapApiLanesStamped.
const MapApiLanesStamped& GetSamplePtrValue(const void* const void_ptr)
{
    auto* const typed_ptr = static_cast<const MapApiLanesStamped*>(void_ptr);
    return *typed_ptr;
}

/// \brief Function that extracts the underlying pointer to const from a SamplePtr and casts away the const. Only used
/// in death test to check that we can't modify the SamplePtr!
template <typename SampleType>
SampleType* ExtractNonConstPointer(const SamplePtr<SampleType>& sample) noexcept
{
    const SampleType* sample_const_ptr = sample.get();

    // The underlying shared memory in which the SamplePtr is stored (i.e. the data section) is opened read-only by the
    // operating system when we open and mmap the memory into our consumer process. However, the SampleType itself is
    // not a const object (although the SamplePtr holds a pointer to const). The standard states that "Modifying a const
    // object through a non-const access path and referring to a volatile object through a non-volatile glvalue results
    // in undefined behavior." (https://en.cppreference.com/w/cpp/language/const_cast). We are _not_ modifying a const
    // object. We are modifying a non-const object that is pointer to by a pointer to const. Therefore, modifying the
    // underlying object after using const cast is not undefined behaviour. We expect that the failure should occur
    // since the memory in which the object is allocated is in read-only memory.
    auto* sample_non_const_ptr = const_cast<SampleType*>(sample_const_ptr);
    return sample_non_const_ptr;
}

void ModifySampleValue(const SamplePtr<MapApiLanesStamped>& sample)
{
    auto* const sample_non_const_ptr = ExtractNonConstPointer(sample);
    sample_non_const_ptr->x += 1;
}

void ModifySampleValue(const SamplePtr<void>& sample)
{
    auto* const sample_non_const_ptr = ExtractNonConstPointer(sample);

    auto* const typed_ptr = static_cast<MapApiLanesStamped*>(sample_non_const_ptr);
    typed_ptr->x += 1;
}

template <typename ProxyType = IpcBridgeProxy>
score::Result<impl::HandleType> GetHandleFromSpecifier(const InstanceSpecifier& instance_specifier)
{
    std::cout << ToString(instance_specifier, ": Running as proxy, looking for services\n");
    ServiceHandleContainer<impl::HandleType> handles{};
    do
    {
        auto handles_result = ProxyType::FindService(instance_specifier);
        if (!handles_result.has_value())
        {
            return MakeUnexpected<impl::HandleType>(std::move(handles_result.error()));
        }
        handles = std::move(handles_result).value();
        if (handles.size() == 0)
        {
            std::this_thread::sleep_for(500ms);
        }
    } while (handles.size() == 0);

    std::cout << ToString(instance_specifier, ": Found service, instantiating proxy\n");
    return handles.front();
}

Result<SampleAllocateePtr<MapApiLanesStamped>> PrepareMapLaneSample(IpcBridgeSkeleton& skeleton,
                                                                    const std::size_t cycle)
{
    const std::default_random_engine::result_type seed{static_cast<std::default_random_engine::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count())};
    std::default_random_engine rng{seed};

    auto sample_result = skeleton.map_api_lanes_stamped_.Allocate();
    if (!sample_result.has_value())
    {
        return sample_result;
    }
    auto sample = std::move(sample_result).value();
    sample->hash_value = START_HASH;
    sample->x = static_cast<std::uint32_t>(cycle);

    std::cout << ToString("Sending sample: ", sample->x, "\n");
    for (MapApiLaneData& lane : sample->lanes)
    {
        for (LaneIdType& successor : lane.successor_lanes)
        {
            successor = std::uniform_int_distribution<std::size_t>()(rng);
        }

        HashArray(lane.successor_lanes, sample->hash_value);
    }
    return sample;
}

}  // namespace

template <typename ProxyType, typename ProxyEventType>
int EventSenderReceiver::RunAsProxy(const score::mw::com::InstanceSpecifier& instance_specifier,
                                    const score::cpp::optional<std::chrono::milliseconds> cycle_time,
                                    const std::size_t num_cycles,
                                    bool try_writing_to_data_segment,
                                    bool check_sample_hash)
{
    // For a GenericProxy, the SampleType will be void. For a regular proxy, it will by MapApiLanesStamped.
    using SampleType =
        typename std::conditional<std::is_same<ProxyType, GenericProxy>::value, void, MapApiLanesStamped>::type;
    constexpr std::size_t SAMPLES_PER_CYCLE = 2U;

    auto handle_result = GetHandleFromSpecifier(instance_specifier);
    if (!handle_result.has_value())
    {
        std::cerr << "Unable to find service: " << instance_specifier
                  << ". Failed with error: " << handle_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto handle = handle_result.value();

    auto proxy_result = ProxyType::Create(std::move(handle));
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to construct proxy: " << proxy_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& proxy = proxy_result.value();

    auto map_api_lanes_stamped_event_optional = GetMapApiLanesStampedProxyEvent(proxy);
    if (!map_api_lanes_stamped_event_optional.has_value())
    {
        std::cerr << "Could not get MapApiLanesStamped proxy event\n";
        return EXIT_FAILURE;
    }
    auto& map_api_lanes_stamped_event = map_api_lanes_stamped_event_optional.value().get();

    concurrency::Notification event_received;
    if (!cycle_time.has_value())
    {
        map_api_lanes_stamped_event.SetReceiveHandler([&event_received, &instance_specifier]() {
            std::cout << ToString(instance_specifier, ": Callback called\n");
            event_received.notify();
        });
    }

    std::cout << ToString(instance_specifier, ": Subscribing to service\n");
    map_api_lanes_stamped_event.Subscribe(SAMPLES_PER_CYCLE);

    score::cpp::optional<char> last_received{};
    SampleReceiver receiver{instance_specifier, check_sample_hash};
    for (std::size_t cycle = 0U; cycle < num_cycles;)
    {
        const auto cycle_start_time = std::chrono::steady_clock::now();
        if (cycle_time.has_value())
        {
            std::this_thread::sleep_for(*cycle_time);
        }

        const auto received_before = receiver.GetReceivedSampleCount();
        Result<std::size_t> num_samples_received = map_api_lanes_stamped_event.GetNewSamples(
            [&receiver, try_writing_to_data_segment](SamplePtr<SampleType> sample) noexcept {
                if (try_writing_to_data_segment)
                {
                    // Try writing to the data segment (in which the sample data is stored). Used in a death test to
                    // ensure that this is not possible.
                    ModifySampleValue(sample);
                }

                // For the GenericProxy case, the void pointer managed by the SamplePtr<void> will be cast to
                // MapApiLanesStamped.
                const MapApiLanesStamped& sample_value = GetSamplePtrValue(sample.get());
                receiver.ReceiveSample(sample_value);
            },
            SAMPLES_PER_CYCLE);
        const auto received = receiver.GetReceivedSampleCount() - received_before;

        const bool get_new_samples_api_error = !num_samples_received.has_value();
        const bool mismatch_api_returned_receive_count_vs_sample_callbacks = *num_samples_received != received;
        const bool receive_handler_called_without_new_samples = *num_samples_received == 0 && !cycle_time.has_value();

        if (get_new_samples_api_error || mismatch_api_returned_receive_count_vs_sample_callbacks ||
            receive_handler_called_without_new_samples)
        {
            std::stringstream ss;
            ss << instance_specifier << ": Error in cycle " << cycle << " during sample reception: ";
            if (!get_new_samples_api_error)
            {
                if (mismatch_api_returned_receive_count_vs_sample_callbacks)
                {
                    ss << "number of received samples doesn't match to what IPC claims: " << *num_samples_received
                       << " vs " << received;
                }
                else
                {
                    ss << "expected at least one new sample, since event-notifier has been called, but "
                          "GetNewSamples() didn't provide one! ";
                }
            }
            else
            {
                ss << std::move(num_samples_received).error();
            }
            ss << ", terminating.\n";
            std::cerr << ss.str();

            map_api_lanes_stamped_event.Unsubscribe();
            return EXIT_FAILURE;
        }

        if (*num_samples_received >= 1U)
        {
            std::cout << ToString(instance_specifier, ": Proxy received valid data\n");
            cycle += *num_samples_received;
        }

        const auto cycle_duration = std::chrono::steady_clock::now() - cycle_start_time;

        std::cout << ToString(instance_specifier,
                              ": Cycle duration ",
                              std::chrono::duration_cast<std::chrono::milliseconds>(cycle_duration).count(),
                              "ms\n");

        event_received.reset();
    }

    std::cout << ToString(instance_specifier, ": Unsubscribing...\n");
    map_api_lanes_stamped_event.Unsubscribe();
    std::cout << ToString(instance_specifier, ": and terminating, bye bye\n");
    return EXIT_SUCCESS;
}

int EventSenderReceiver::RunAsSkeleton(const score::mw::com::InstanceSpecifier& instance_specifier,
                                       const std::chrono::milliseconds cycle_time,
                                       const std::size_t num_cycles)
{
    auto create_result = IpcBridgeSkeleton::Create(instance_specifier);
    if (!create_result.has_value())
    {
        std::cerr << "Unable to construct skeleton: " << create_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& skeleton = create_result.value();

    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for skeleton: " << offer_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    std::cout << "Starting to send data\n";

    for (std::size_t cycle = 0U; cycle < num_cycles || num_cycles == 0U; ++cycle)
    {
        auto sample_result = PrepareMapLaneSample(skeleton, cycle);
        if (!sample_result.has_value())
        {
            std::cerr << "No sample received. Exiting.\n";
            return EXIT_FAILURE;
        }
        auto sample = std::move(sample_result).value();

        {
            std::lock_guard lock{event_sending_mutex_};
            skeleton.map_api_lanes_stamped_.Send(std::move(sample));
            event_published_ = true;
        }
        std::this_thread::sleep_for(cycle_time);
    }

    std::cout << "Stop offering service...";
    skeleton.StopOfferService();
    std::cout << "and terminating, bye bye\n";

    return EXIT_SUCCESS;
}

template int EventSenderReceiver::RunAsProxy<IpcBridgeProxy, impl::ProxyEvent<MapApiLanesStamped>>(
    const score::mw::com::InstanceSpecifier&,
    const score::cpp::optional<std::chrono::milliseconds>,
    const std::size_t,
    bool,
    bool);
template int EventSenderReceiver::RunAsProxy<impl::GenericProxy, impl::GenericProxyEvent>(
    const score::mw::com::InstanceSpecifier&,
    const score::cpp::optional<std::chrono::milliseconds>,
    const std::size_t,
    bool,
    bool);

}  // namespace score::mw::com
