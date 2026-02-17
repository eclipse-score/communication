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

#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/impl/handle_type.h"

#include "score/concurrency/notification.h"
#include "score/memory/shared/vector.h"
#include "score/os/mman.h"

#include "score/mw/com/impl/proxy_event.h"
#include <score/assert.hpp>
#include <score/hash.hpp>
#include <score/optional.hpp>

#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

using namespace std::chrono_literals;

namespace score::mw::com::test
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

class MmanMock : public os::Mman
{
  public:
    score::cpp::expected<void*, os::Error> mmap(void* addr,
                                         const std::size_t length,
                                         const Protection protection,
                                         const Map flags,
                                         const std::int32_t fd,
                                         const std::int64_t offset) const noexcept override
    {
        // mmap calls are uninteresting and are forwarded directly
        return reinterpret_cast<os::internal::MmanImpl&>(
                   os::StaticDestructionGuard<os::internal::MmanImpl>::GetStorage())
            .mmap(addr, length, protection, flags, fd, offset);
    };

    score::cpp::expected_blank<os::Error> munmap(void* addr, const std::size_t length) const noexcept override
    {
        // munmap calls are uninteresting and are forwarded directly
        return reinterpret_cast<os::internal::MmanImpl&>(
                   os::StaticDestructionGuard<os::internal::MmanImpl>::GetStorage())
            .munmap(addr, length);
    };

    score::cpp::expected<std::int32_t, os::Error> shm_open(const char* pathname,
                                                    const os::Fcntl::Open oflag,
                                                    const os::Stat::Mode mode) const noexcept override
    {
        // shm_open calls are INTERESTING for this test - we memorize the pathname - and then forward
        std::strcpy(last_shm_open_path_, pathname);
        shm_open_callcount_++;
        return reinterpret_cast<os::internal::MmanImpl&>(
                   os::StaticDestructionGuard<os::internal::MmanImpl>::GetStorage())
            .shm_open(pathname, oflag, mode);
    };

    score::cpp::expected_blank<os::Error> shm_unlink(const char* pathname) const noexcept override
    {
        // shm_unlink calls are uninteresting and are forwarded directly
        return reinterpret_cast<os::internal::MmanImpl&>(
                   os::StaticDestructionGuard<os::internal::MmanImpl>::GetStorage())
            .shm_unlink(pathname);
    };

#if defined(__EXT_POSIX1_200112)
    score::cpp::expected<std::int32_t, os::Error> posix_typed_mem_open(
        const char* name,
        const os::Fcntl::Open oflag,
        const os::Mman::PosixTypedMem tflag) const noexcept override
    {
        return reinterpret_cast<os::internal::MmanImpl&>(
                   os::StaticDestructionGuard<os::internal::MmanImpl>::GetStorage())
            .posix_typed_mem_open(name, oflag, tflag);
    }

    score::cpp::expected<std::int32_t, os::Error> posix_typed_mem_get_info(
        const std::int32_t fd,
        struct posix_typed_mem_info* info) const noexcept override
    {
        return reinterpret_cast<os::internal::MmanImpl&>(
                   os::StaticDestructionGuard<os::internal::MmanImpl>::GetStorage())
            .posix_typed_mem_get_info(fd, info);
    }

#endif

    std::uint32_t GetShmOpenCallcount() const noexcept
    {
        return shm_open_callcount_;
    }

    const char* GetLastShmOpenPath() const noexcept
    {
        return last_shm_open_path_;
    }

  private:
    mutable char last_shm_open_path_[1024];
    mutable std::uint32_t shm_open_callcount_;
};

class SampleReceiver
{
  public:
    SampleReceiver(const score::mw::com::InstanceSpecifier& instance_specifier)
        : instance_specifier_{instance_specifier}, last_received_{}, received_{0U}
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

        return true;
    }

    const score::mw::com::InstanceSpecifier& instance_specifier_;
    score::cpp::optional<std::uint32_t> last_received_;
    std::size_t received_;
};

enum class TestErrorCode : score::result::ErrorCode
{
    kStopRequested = 1,
};

class TestErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override
    {
        switch (static_cast<TestErrorCode>(code))
        {
            case TestErrorCode::kStopRequested:
                return "Stop was requested by stop token.";
            default:
                return "Unknown Error!";
        }
    }
};

constexpr TestErrorDomain test_error_domain;

score::result::Error MakeError(const TestErrorCode code, const std::string_view user_message = "") noexcept
{
    return {static_cast<score::result::ErrorCode>(code), test_error_domain, user_message};
}

class TestDestructor
{
  public:
    TestDestructor(score::cpp::stop_source& stop_source) : stop_source_{stop_source} {}
    ~TestDestructor()
    {
        stop_source_.request_stop();
    }

  private:
    score::cpp::stop_source stop_source_;
};

template <typename SampleType>
bool ElementFqIdMatchesConfigurationValue(
    score::mw::com::impl::ProxyEvent<SampleType>& proxy_event,
    const score::mw::com::impl::lola::ElementFqId element_fq_id_from_config) noexcept
{
    auto* const binding = impl::ProxyEventView<SampleType>{proxy_event}.GetBinding();
    auto* const lola_binding = dynamic_cast<impl::lola::ProxyEvent<SampleType>*>(binding);
    if (lola_binding == nullptr)
    {
        return {};
    }
    const auto element_fq_id = lola_binding->GetElementFQId();
    if (element_fq_id.ToString() != element_fq_id_from_config.ToString())
    {
        std::cerr << "Generated ElementFqId does not match that specified in the configuration." << std::endl;
        return false;
    }
    return true;
}

/// @brief Function to check whether a file exists which works on linux and QNX
bool FileExists(const std::string& filePath)
{
    return access(filePath.c_str(), F_OK) == 0;
}

score::cpp::optional<std::reference_wrapper<impl::ProxyEvent<MapApiLanesStamped>>> GetMapApiLanesStampedProxyEvent(
    BigDataProxy& proxy)
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

template <typename ProxyType = BigDataProxy>
score::Result<impl::HandleType> GetHandleFromSpecifier(const InstanceSpecifier& instance_specifier,
                                                     const score::cpp::stop_token& stop_token)
{
    std::cout << ToString(instance_specifier, ": Running as proxy, looking for services\n");
    ServiceHandleContainer<impl::HandleType> handles{};
    do
    {
        if (stop_token.stop_requested())
        {
            return MakeUnexpected(TestErrorCode::kStopRequested);
        }
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

Result<SampleAllocateePtr<MapApiLanesStamped>> PrepareMapLaneSample(BigDataSkeleton& bigdata, const std::size_t cycle)
{
    const std::default_random_engine::result_type seed{static_cast<std::default_random_engine::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count())};
    std::default_random_engine rng{seed};

    auto sample_result = bigdata.map_api_lanes_stamped_.Allocate();
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
                                    const score::cpp::stop_token& stop_token,
                                    bool try_writing_to_data_segment)
{
    // For a GenericProxy, the SampleType will be void. For a regular proxy, it will by MapApiLanesStamped.
    using SampleType =
        typename std::conditional<std::is_same<ProxyType, GenericProxy>::value, void, MapApiLanesStamped>::type;
    constexpr const std::size_t SAMPLES_PER_CYCLE = 2U;

    auto handle_result = GetHandleFromSpecifier(instance_specifier, stop_token);
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
        std::cerr << "Unable to construct BigDataProxy: " << proxy_result.error() << ", bailing!\n";
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

    SampleReceiver receiver{instance_specifier};
    for (std::size_t cycle = 0U; (cycle < num_cycles) && !stop_token.stop_requested();)
    {
        const auto cycle_start_time = std::chrono::steady_clock::now();
        if (cycle_time.has_value())
        {
            std::this_thread::sleep_for(*cycle_time);
        }
        else
        {
            if (!event_received.waitWithAbort(stop_token))
            {
                // Abort happened
                break;
            }
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
                                       const std::size_t num_cycles,
                                       const score::cpp::stop_token& stop_token)
{
    auto bigdata_result = BigDataSkeleton::Create(instance_specifier);
    if (!bigdata_result.has_value())
    {
        std::cerr << "Unable to construct BigDataSkeleton: " << bigdata_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = bigdata_result.value();

    const auto offer_result = bigdata.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for BigDataSkeleton: " << offer_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    std::cout << "Starting to send data\n";

    for (std::size_t cycle = 0U; (cycle < num_cycles || num_cycles == 0U) && !stop_token.stop_requested(); ++cycle)
    {
        auto sample_result = PrepareMapLaneSample(bigdata, cycle);
        if (!sample_result.has_value())
        {
            std::cerr << "No sample received. Exiting.\n";
            return EXIT_FAILURE;
        }
        auto sample = std::move(sample_result).value();

        {
            std::lock_guard<std::mutex> lock{event_sending_mutex_};
            bigdata.map_api_lanes_stamped_.Send(std::move(sample));
            event_published_ = true;
        }
        std::this_thread::sleep_for(cycle_time);
    }

    std::cout << "Stop offering service...";
    bigdata.StopOfferService();
    std::cout << "and terminating, bye bye\n";

    skeleton_finished_publishing_.notify();

    return EXIT_SUCCESS;
}

int EventSenderReceiver::RunAsSkeletonWaitForProxy(const score::mw::com::InstanceSpecifier& instance_specifier,
                                                   score::os::InterprocessNotification& interprocess_notification,
                                                   const score::cpp::stop_token& stop_token)
{
    auto bigdata_result = BigDataSkeleton::Create(instance_specifier);
    if (!bigdata_result.has_value())
    {
        std::cerr << "Unable to construct BigDataSkeleton: " << bigdata_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = bigdata_result.value();

    const auto offer_result = bigdata.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for BigDataSkeleton: " << offer_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    std::cout << "Starting to send data\n";

    // Wait until proxy has finished before exiting.
    if (!interprocess_notification.waitWithAbort(stop_token))
    {
        // Abort happened
        std::cerr << "Request stop on stop token. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Stop offering service...";
    bigdata.StopOfferService();
    std::cout << "and terminating, bye bye\n";

    return EXIT_SUCCESS;
}

int EventSenderReceiver::RunAsSkeletonCheckEventSlots(const score::mw::com::InstanceSpecifier& instance_specifier,
                                                      const std::uint16_t num_skeleton_slots,
                                                      score::cpp::stop_source stop_source)
{
    // Notify the proxy that it can finish when this function exits.
    TestDestructor test_destructor(stop_source);

    const score::cpp::stop_token stop_token = stop_source.get_token();

    auto bigdata_result = BigDataSkeleton::Create(instance_specifier);
    if (!bigdata_result.has_value())
    {
        std::cerr << "Unable to construct BigDataSkeleton: " << bigdata_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = bigdata_result.value();

    const auto offer_result = bigdata.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for BigDataSkeleton: " << offer_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }

    // Wait until proxy is ready
    if (!proxy_ready_to_receive_.waitWithAbort(stop_token))
    {
        // Abort happened
        std::cerr << "Request stop on stop token. Exiting\n";
        return EXIT_FAILURE;
    }
    std::cout << "Starting to send data\n";

    // Publish events until all slots are allocated
    std::size_t cycle{0};
    bool test_passed{false};
    while (!stop_token.stop_requested())
    {
        std::unique_lock<std::mutex> lock{map_lanes_mutex_};
        std::size_t num_events_stored = map_lanes_list_.size();
        auto sample_result = PrepareMapLaneSample(bigdata, cycle);
        lock.unlock();

        const bool slots_filled = (num_events_stored < num_skeleton_slots);
        if (slots_filled)  // Slots aren't all filled
        {
            if (!sample_result.has_value())
            {
                std::cerr << "Unable to allocate slot. Exiting.\n";
                break;
            }
            else
            {
                bigdata.map_api_lanes_stamped_.Send(std::move(sample_result).value());
            }
        }
        else  // All event slots have already been allocated
        {
            // If all the event slots have already been allocated, we shouldn't have been able to allocate another slot.
            if (sample_result.has_value())
            {
                std::cerr << "More Slots were allocated than specified in the configuration. Exiting.\n";
                break;
            }
            else
            {
                // Slot wasn't allocated.
                std::cout
                    << "Test passed: Max number of slots were allocated and then additional allocate calls fail\n";
                test_passed = true;
                break;
            }
        }

        // Wait until the proxy is ready to receive another event
        if (!proxy_event_received_.waitWithAbort(stop_token))
        {
            // Abort happened
            break;
        }
        proxy_event_received_.reset();

        ++cycle;
    }

    std::cout << "Stop offering service...";
    bigdata.StopOfferService();
    std::cout << "and terminating, bye bye\n";

    return test_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

int EventSenderReceiver::RunAsSkeletonCheckValuesCreatedFromConfig(
    const score::mw::com::InstanceSpecifier& instance_specifier,
    const std::string& shared_memory_path,
    score::os::InterprocessNotification& interprocess_notification_from_proxy,
    score::os::InterprocessNotification& interprocess_notification_to_proxy,
    score::cpp::stop_source stop_source)
{
    TestDestructor test_destructor(stop_source);
    score::cpp::stop_token stop_token = stop_source.get_token();

    // The shared memory path should only be created when a BigDataSkeleton object is instantiated.
    if (FileExists(shared_memory_path))
    {
        std::cerr << "Shared memory file already exists. Exiting.\n";
        return EXIT_FAILURE;
    }

    auto bigdata_result = BigDataSkeleton::Create(instance_specifier);
    if (!bigdata_result.has_value())
    {
        std::cerr << "Unable to construct BigDataSkeleton: " << bigdata_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = bigdata_result.value();

    const auto offer_service_result = bigdata.OfferService();
    if (!offer_service_result.has_value())
    {
        std::cerr << "Unable to offer service for BigDataSkeleton: " << offer_service_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }

    if (!FileExists(shared_memory_path))
    {
        std::cerr << "Shared memory file was not created by skeleton. Exiting.\n";
        return EXIT_FAILURE;
    }

    interprocess_notification_to_proxy.notify();

    // Wait until proxy has finished before exiting.
    if (!interprocess_notification_from_proxy.waitWithAbort(stop_token))
    {
        // Abort happened
        std::cerr << "Request stop on stop token. Exiting.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Stop offering service...";
    bigdata.StopOfferService();
    std::cout << "and terminating, bye bye\n";

    return EXIT_SUCCESS;
}

int EventSenderReceiver::RunAsProxyCheckValuesCreatedFromConfig(
    const score::mw::com::InstanceSpecifier& instance_specifier,
    const score::mw::com::impl::lola::ElementFqId map_api_lanes_element_fq_id_from_config,
    const score::mw::com::impl::lola::ElementFqId dummy_data_element_fq_id_from_config,
    const std::string& shared_memory_path,
    score::os::InterprocessNotification& interprocess_notification_from_skeleton,
    score::os::InterprocessNotification& interprocess_notification_to_skeleton,
    score::cpp::stop_token stop_token)
{
    // create special mmap mock to intercept/catch shm_open calls
    MmanMock mman_mock{};
    // activate our mock BEFORE instantiating BigDataProxy, which will lead to opening/mapping shm.
    os::Mman::set_testing_instance(mman_mock);

    if (!interprocess_notification_from_skeleton.waitWithAbort(stop_token))
    {
        // Abort happened
        std::cerr << "Request stop on stop token. Exiting.\n";
        return EXIT_FAILURE;
    }

    auto handle_result = GetHandleFromSpecifier(instance_specifier, stop_token);
    if (!handle_result.has_value())
    {
        std::cerr << "Unable to find service: " << instance_specifier
                  << ". Failed with error: " << handle_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto handle = handle_result.value();

    auto proxy_result = BigDataProxy::Create(std::move(handle));
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to construct BigDataProxy: " << proxy_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = proxy_result.value();

    // Remove it again
    os::Mman::restore_instance();
    // verify, that shm_opem has been called with the expected/correct path
    if (std::strcmp(mman_mock.GetLastShmOpenPath(), shared_memory_path.c_str()) != 0)
    {
        std::cerr << "Shared memory file was not opened by proxy under expected name: " << shared_memory_path.c_str()
                  << " but instead: " << mman_mock.GetLastShmOpenPath() << " ... Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    bigdata.map_api_lanes_stamped_.Subscribe(2);

    // Check that the ElementFqId used for the events match the values manually generated from the configuration.
    if (!ElementFqIdMatchesConfigurationValue(bigdata.map_api_lanes_stamped_, map_api_lanes_element_fq_id_from_config))
    {
        std::cerr << "map_api_lanes_stamped ElementFqId is different from that specified in configuration."
                  << std::endl;
        return EXIT_FAILURE;
    }

    if (!ElementFqIdMatchesConfigurationValue(bigdata.dummy_data_stamped_, dummy_data_element_fq_id_from_config))
    {
        std::cerr << "dummy_data_stamped ElementFqId is different from that specified in configuration." << std::endl;
        return EXIT_FAILURE;
    }

    // Wait for the subscription to finish before unsubscribing.
    while (bigdata.map_api_lanes_stamped_.GetSubscriptionState() != SubscriptionState::kSubscribed)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Unsubscribing...";
    bigdata.map_api_lanes_stamped_.Unsubscribe();
    std::cout << "and terminating, bye bye\n";

    // Tell the skeleton that the proxy has finished.
    interprocess_notification_to_skeleton.notify();

    return EXIT_SUCCESS;
}

int EventSenderReceiver::RunAsProxyReceiveHandlerOnly(const score::mw::com::InstanceSpecifier& instance_specifier,
                                                      const score::cpp::stop_token& stop_token)
{
    constexpr const std::size_t SAMPLES_PER_CYCLE = 2U;

    auto handle_result = GetHandleFromSpecifier(instance_specifier, stop_token);
    if (!handle_result.has_value())
    {
        std::cerr << "Unable to find service: " << instance_specifier
                  << ". Failed with error: " << handle_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto handle = handle_result.value();

    auto proxy_result = BigDataProxy::Create(std::move(handle));
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to construct BigDataProxy: " << proxy_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = proxy_result.value();

    std::atomic<bool> callback_called{false};
    bigdata.map_api_lanes_stamped_.SetReceiveHandler([&callback_called]() {
        callback_called = true;
        std::cout << "Callback called\n";
    });

    std::cout << "Subscribing to service\n";
    bigdata.map_api_lanes_stamped_.Subscribe(SAMPLES_PER_CYCLE);

    // Make sure that callback is called at least once. If stop is called via the stop_token or the skeleton notifies
    // that it's finished publishing, exit with a failure code.
    while (!callback_called)
    {
        const auto sleep_duration = std::chrono::milliseconds{100};
        const auto abort_called = skeleton_finished_publishing_.waitForWithAbort(sleep_duration, stop_token);
        if (abort_called)
        {
            return EXIT_FAILURE;
        }
    }

    // Reset the callback flag and unsubscribe within the mutex that prevents the skeleton sending any events. This is
    // to ensure that we don't get any events published (and subsequently callbacks called) between clearing the
    // callback-called flag and calling unsubscribe.
    {
        std::lock_guard<std::mutex> lock{event_sending_mutex_};
        callback_called = false;
        event_published_ = false;
        bigdata.map_api_lanes_stamped_.Unsubscribe();
    }

    // Wait until the skeleton has finished publishing all events.
    score::cpp::ignore = skeleton_finished_publishing_.waitWithAbort(stop_token);

    // Make sure that at least one event was published since calling Unsubscribe().
    if (!event_published_)
    {
        std::cerr << "No event was published after Unsubscribe(), test is invalid. Terminating!\n";
        return EXIT_FAILURE;
    }
    if (callback_called)
    {
        std::cerr << "Callback was called after Unsubscribe(), terminating!\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int EventSenderReceiver::RunAsProxyCheckEventSlots(const score::mw::com::InstanceSpecifier& instance_specifier,
                                                   const std::uint16_t num_proxy_slots,
                                                   score::cpp::stop_token stop_token)
{
    auto handle_result = GetHandleFromSpecifier(instance_specifier, stop_token);
    if (!handle_result.has_value())
    {
        std::cerr << "Unable to find service: " << instance_specifier
                  << ". Failed with error: " << handle_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto handle = handle_result.value();

    auto proxy_result = BigDataProxy::Create(std::move(handle));
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to construct BigDataProxy: " << proxy_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = proxy_result.value();

    concurrency::Notification event_received;
    bigdata.map_api_lanes_stamped_.SetReceiveHandler([&event_received]() {
        std::cout << "Callback called\n";
        event_received.notify();
    });

    std::cout << "Subscribing to service\n";
    bigdata.map_api_lanes_stamped_.Subscribe(num_proxy_slots);

    // Ensure that receive handler is set before the skeleton begins publishing events.
    proxy_ready_to_receive_.notify();
    while (!stop_token.stop_requested())
    {
        if (!event_received.waitWithAbort(stop_token))
        {
            // Abort happened
            break;
        }

        // Store SamplePtr in a vector so that it persists and the ref count of that slot is not decremented.
        Result<std::size_t> num_samples_received = bigdata.map_api_lanes_stamped_.GetNewSamples(
            [this](SamplePtr<MapApiLanesStamped> sample) noexcept {
                std::lock_guard<std::mutex> lock{map_lanes_mutex_};
                map_lanes_list_.push_back(std::move(sample));
            },
            num_proxy_slots);

        if (!num_samples_received.has_value())
        {
            std::cerr << "Unable to get new samples: " << num_samples_received.error() << ", bailing!\n";
            return EXIT_FAILURE;
        }

        event_received.reset();

        // Tell skeleton that we are ready to receive the next event.
        proxy_event_received_.notify();
    }

    {
        std::lock_guard<std::mutex> lock{map_lanes_mutex_};
        map_lanes_list_.clear();
    }

    std::cout << "Unsubscribing...";
    bigdata.map_api_lanes_stamped_.Unsubscribe();
    std::cout << "and terminating, bye bye\n";
    return EXIT_SUCCESS;
}

int EventSenderReceiver::RunAsProxyCheckSubscribeHandler(const score::mw::com::InstanceSpecifier& instance_specifier,
                                                         score::os::InterprocessNotification& interprocess_notification,
                                                         score::cpp::stop_token stop_token)
{
    constexpr const std::size_t SAMPLES_PER_CYCLE = 2U;

    auto handle_result = GetHandleFromSpecifier(instance_specifier, stop_token);
    if (!handle_result.has_value())
    {
        std::cerr << "Unable to find service: " << instance_specifier
                  << ". Failed with error: " << handle_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto handle = handle_result.value();

    auto proxy_result = BigDataProxy::Create(std::move(handle));
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to construct BigDataProxy: " << proxy_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& bigdata = proxy_result.value();

    bigdata.map_api_lanes_stamped_.Subscribe(SAMPLES_PER_CYCLE);
    bigdata.map_api_lanes_stamped_.Unsubscribe();

    interprocess_notification.notify();

    return EXIT_SUCCESS;
}

template int EventSenderReceiver::RunAsProxy<BigDataProxy, impl::ProxyEvent<MapApiLanesStamped>>(
    const score::mw::com::InstanceSpecifier&,
    const score::cpp::optional<std::chrono::milliseconds>,
    const std::size_t,
    const score::cpp::stop_token&,
    bool try_writing_to_data_segment);
template int EventSenderReceiver::RunAsProxy<impl::GenericProxy, impl::GenericProxyEvent>(
    const score::mw::com::InstanceSpecifier&,
    const score::cpp::optional<std::chrono::milliseconds>,
    const std::size_t,
    const score::cpp::stop_token&,
    bool try_writing_to_data_segment);

}  // namespace score::mw::com::test
