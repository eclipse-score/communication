#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/generic_skeleton.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include "score/mw/log/logging.h"

#include <score/stop_token.hpp>

#include "score/result/result.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// Default to 64-byte if not specified by the build system
#ifndef PAYLOAD_SIZE
#define PAYLOAD_SIZE 64
#endif

namespace
{

struct MyEventData
{
    std::uint64_t counter;
#if PAYLOAD_SIZE > 8
    char padding[PAYLOAD_SIZE - 8];
#endif
};

constexpr std::string_view kInstanceSpecifier = "/test/generic/generic/interaction";
constexpr std::uint64_t kSamplesToProcess = 30;
constexpr int kSamplesToSubscribe = 5;

#if PAYLOAD_SIZE == 64
constexpr std::string_view kEventName = "Event64Byte";
#elif PAYLOAD_SIZE == 32
constexpr std::string_view kEventName = "Event32Byte";
#elif PAYLOAD_SIZE == 16
constexpr std::string_view kEventName = "Event16Byte";
#elif PAYLOAD_SIZE == 8
constexpr std::string_view kEventName = "Event8Byte";
#else
#error "Unsupported payload size configured."
#endif

int run_provider(score::cpp::stop_token stop_token)
{
    const auto instance_specifier = score::mw::com::impl::InstanceSpecifier::Create(kInstanceSpecifier).value();
    std::cout << "[PROVIDER] Instance specifier created." << std::endl;
    const score::mw::com::impl::DataTypeMetaInfo meta{sizeof(MyEventData), alignof(MyEventData)};
    std::cout << "[PROVIDER] DataTypeMetaInfo created (size=" << sizeof(MyEventData)
              << ", align=" << alignof(MyEventData) << ")." << std::endl;
    const std::vector<score::mw::com::impl::EventInfo> events = {{kEventName, meta}};
    std::cout << "[PROVIDER] EventInfo vector created for event: " << kEventName << std::endl;

    score::mw::com::impl::GenericSkeletonServiceElementInfo create_params;
    create_params.events = events;
    std::cout << "[PROVIDER] GenericSkeletonServiceElementInfo prepared." << std::endl;

    std::cout << "[PROVIDER] Calling GenericSkeleton::Create..." << std::endl;
    auto skeleton_res = score::mw::com::impl::GenericSkeleton::Create(instance_specifier, create_params);
    if (!skeleton_res.has_value())
    {
        std::cerr << "[PROVIDER] GenericSkeleton::Create FAILED." << std::endl;
        return 1;
    }
    auto& skeleton = skeleton_res.value();
    std::cout << "[PROVIDER] GenericSkeleton created." << std::endl;

    std::cout << "[PROVIDER] Calling skeleton.OfferService()..." << std::endl;
    if (!skeleton.OfferService().has_value())
    {
        std::cerr << "[PROVIDER] OfferService FAILED." << std::endl;
        return 1;
    }
    std::cout << "[PROVIDER] OfferService SUCCEEDED." << std::endl;

    std::cout << "[PROVIDER] Getting event reference for " << kEventName << "..." << std::endl;
    auto it = skeleton.GetEvents().find(kEventName);
    if (it == skeleton.GetEvents().cend())
    {
        std::cerr << "[PROVIDER] Could not find event: " << kEventName << std::endl;
        return 1;
    }
    std::cout << "[PROVIDER] Event reference obtained." << std::endl;

    // Get reference to the GenericSkeletonEvent
    auto& generic_event = const_cast<score::mw::com::impl::GenericSkeletonEvent&>(it->second);

    std::cout << "[PROVIDER] Generic-Generic " << PAYLOAD_SIZE << "-byte - Waiting 5s for consumer to subscribe..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "[PROVIDER] Finished initial 5s sleep." << std::endl;

    std::uint64_t i{0};
    while (!stop_token.stop_requested())
    {
        auto sample_res = generic_event.Allocate();
        if (!sample_res.has_value())
        {
            std::cerr << "[PROVIDER] Allocation failed for sample: " << i << std::endl;
            return 1;
        }
        std::cout << "[PROVIDER] Sample " << i << " allocated." << std::endl;

        auto* typed_sample = static_cast<MyEventData*>(sample_res.value().Get());
        typed_sample->counter = i;

        std::cout << "[PROVIDER] Sending sample: " << i << std::endl;
        generic_event.Send(std::move(sample_res.value()));
        std::cout << "[PROVIDER] " << PAYLOAD_SIZE << "-byte Event Sent sample: " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        i++;
    }

    std::cout << "[PROVIDER] Received termination signal. Calling StopOfferService()..." << std::endl;
    skeleton.StopOfferService();
    std::cout << "[PROVIDER] StopOfferService() completed." << std::endl;
    return 0;
}

int run_consumer()
{
    const auto instance_specifier = score::mw::com::impl::InstanceSpecifier::Create(kInstanceSpecifier).value();

    score::Result<score::mw::com::ServiceHandleContainer<score::mw::com::impl::GenericProxy::HandleType>> handles_res;
    int retries{0};
    while (retries < 50)
    {
        handles_res = score::mw::com::impl::GenericProxy::FindService(instance_specifier);
        if (handles_res.has_value() && !handles_res.value().empty())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries++;
    }
    if (!handles_res.has_value() || handles_res.value().empty())
        return 1;

    auto proxy_res = score::mw::com::impl::GenericProxy::Create(handles_res.value()[0]);
    if (!proxy_res.has_value())
        return 1;
    auto& proxy = proxy_res.value();

    auto event_it = proxy.GetEvents().find(kEventName);
    if (event_it == proxy.GetEvents().cend())
        return 1;

    // Get reference to the GenericProxyEvent
    auto& generic_event = event_it->second;
    [[maybe_unused]] score::Result<void> res = generic_event.Subscribe(kSamplesToSubscribe);

    std::uint64_t expected{0};
    std::uint64_t received{0};
    int data_mismatches{0};
    bool is_first_sample{true};

    while (received < kSamplesToProcess)
    {
        // The receiver callback operates on type-erased memory (SamplePtr<const void>)
        generic_event.GetNewSamples(
            [&](auto sample) {
                auto* typed_sample = static_cast<const MyEventData*>(sample.get());

                if (is_first_sample)
                {
                    expected = typed_sample->counter;
                    is_first_sample = false;
                }

                if (typed_sample->counter != expected)
                {
                    std::cerr << "[CONSUMER] " << PAYLOAD_SIZE << "-byte Data mismatch! Expected: " << expected
                              << ", got: " << typed_sample->counter << std::endl;
                    data_mismatches++;
                }
                else
                {
                    std::cout << "[CONSUMER] " << PAYLOAD_SIZE
                              << "-byte Event Received sample: " << typed_sample->counter << std::endl;
                }
                expected++;
                received++;
            },
            kSamplesToSubscribe);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (data_mismatches > 0)
    {
        std::cerr << "[CONSUMER] Test failed with " << data_mismatches << " mismatches." << std::endl;
        return 1;
    }
    return 0;
}
}  // namespace

int main(int argc, const char* argv[])
{
    std::string mode;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--mode" && i + 1 < argc)
            mode = argv[++i];
    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration(argc, argv));

    score::cpp::stop_source stop_source{};
    score::mw::com::SetupStopTokenSigTermHandler(stop_source);

    if (mode == "provider")
        return run_provider(stop_source.get_token());
    if (mode == "consumer")
        return run_consumer();
    return 1;
}
