
#include "score/mw/com/test/loading_add_on_configuration/consumer.h"
#include "score/mw/com/test/loading_add_on_configuration/test_constants.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "types/example_interface.h"

namespace score::mw::com::test
{

void run_consumer(const score::cpp::stop_token& stop_token,
                  const score::mw::com::InstanceSpecifier& instance_specifier,
                  ProcessSynchronizer& process_synchronizer,
                  ProcessSynchronizer& provider_ready_synchronizer,
                  const std::vector<std::uint32_t>& samples)
{
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer]() {
        process_synchronizer.Notify();
    }};

    std::cout << "\nConsumer: Step 1 - Waiting for provider" << std::endl;

    if (!provider_ready_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("Consumer: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }
    // Reset for subsequent calls
    provider_ready_synchronizer.Reset();

    // Step 2. Find service and create proxy
    std::cout << "\nConsumer: Step 2 - Find service and create proxy" << std::endl;

    ProxyContainer<ExampleInterfaceProxy> proxy_container{};
    proxy_container.CreateProxy(instance_specifier, "regular_service");
    auto& proxy = proxy_container.GetProxy();

    ProxyEventReceiver event_receiver{proxy.example_event};
    ProxyEventStateChangeNotifier subscription_notifier{proxy.example_event};

    // Step 3. Subscribe to event with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 3 - Subscribe to event" << std::endl;
    const auto subscribe_result = proxy.example_event.Subscribe(kTotalNumValuesToSend);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed for example_event: ", subscribe_result.error());
    }

    // Step 4. Wait for subscription
    std::cout << "\nConsumer: Step 4 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in event scenario");
    }

    // Step 5. Wait for all expected samples
    std::cout << "\nConsumer: Step 5 - Wait for all expected samples" << std::endl;
    if (!event_receiver.WaitForSamples(stop_token, samples))
    {
        FailTest("Consumer: Did not receive all expected samples in event scenario");
    }

    // Step 6. Notify provider that data was received
    std::cout << "\nConsumer: Step 6 - Notify provider that data was received" << std::endl;

    process_synchronizer.Notify();
}

}  // namespace score::mw::com::test
