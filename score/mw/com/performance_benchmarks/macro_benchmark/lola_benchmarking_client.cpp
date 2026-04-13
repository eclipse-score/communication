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
#include "score/mw/com/performance_benchmarks/macro_benchmark/common_resources.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/config_parser.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/lola_interface.h"
#include "score/mw/com/types.h"
#include "score/mw/log/logging.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

// LogContext BCLI -> BenchmarkClient ;)
constexpr std::string_view kLogContext{"BCli"};

namespace score::mw::com::test
{

namespace
{

using score::mw::com::test::TestDataProxy;
using namespace std::chrono_literals;

bool signal_service_that_client_is_done()
{
    auto proxy_is_done_flag_opt = GetSharedFlag();

    if (!proxy_is_done_flag_opt.has_value())
    {
        return false;
    }

    auto& proxy_is_done_flag = proxy_is_done_flag_opt.value();
    proxy_is_done_flag.GetObject()++;
    proxy_is_done_flag.CleanUp();

    return true;
}

class ServiceFinder
{
    static constexpr auto kFindServiceBackoffTime{1ms};
    ServiceFinderMode service_finder_mode_;
    score::cpp::stop_token stop_token_;

  public:
    ServiceFinder(ServiceFinderMode service_finder_mode, score::cpp::stop_token stop_token)
        : service_finder_mode_{service_finder_mode}, stop_token_{stop_token}
    {
    }

    std::optional<TestDataProxy::HandleType> find(const InstanceSpecifier& instance_specifier)
    {
        if (service_finder_mode_ == ServiceFinderMode::POLLING)
        {
            mw::log::LogInfo(kLogContext) << "Starting find service in polling mode!";
            return PollService(instance_specifier);
        }
        else if (service_finder_mode_ == ServiceFinderMode::ASYNC)
        {
            mw::log::LogInfo(kLogContext) << "Starting find service in async mode!";
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(service_finder_mode_ == ServiceFinderMode::ASYNC,
                                                        "ASYNC mode is not supported yet");
        }
        else
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "Unexpected find service mode!");
        }
        return {};
    }

  private:
    std::optional<TestDataProxy::HandleType> PollService(const InstanceSpecifier& instance_specifier)
    {
        std::optional<TestDataProxy::HandleType> find_service_handle_result;
        while (!find_service_handle_result.has_value())
        {

            auto find_service_result = TestDataProxy::FindService(instance_specifier);
            if (!find_service_result.has_value())
            {
                score::mw::log::LogError(kLogContext)
                    << "Error occurred during FindService call: " << find_service_result.error();
                return std::nullopt;
            }

            auto& service_handle_container = find_service_result.value();

            if (!service_handle_container.empty())
            {
                find_service_handle_result = service_handle_container[0];
                break;
            }

            if (stop_token_.stop_requested())
            {
                return std::nullopt;
            }

            std::this_thread::sleep_for(kFindServiceBackoffTime);
        }
        return find_service_handle_result;
    }
};

enum class ReceptionState : std::uint8_t
{
    RUNNING,
    FINISHED_SUCCESS,
    FINISHED_ERROR,
    ABORTED
};

struct ReceiveHandlerContext
{
    explicit ReceiveHandlerContext(const ClientConfig& client_config) : config{client_config} {}
    unsigned long number_of_samples_received{0U};
    std::mutex mutex{};
    std::condition_variable cv{};
    const ClientConfig& config;
    ReceptionState reception_state{ReceptionState::RUNNING};
};

class RunDurationHandler
{

    std::optional<ClientConfig::RunTimeLimit> run_time_limit_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;

  public:
    RunDurationHandler(const ClientConfig& cc) : run_time_limit_(cc.run_time_limit)
    {
        start_ = std::chrono::high_resolution_clock::now();
    }

    bool run_duration_was_exceeded(unsigned long number_of_samples_received) const
    {
        if (!run_time_limit_.has_value())
        {
            return false;
        }

        const auto now = std::chrono::high_resolution_clock::now();

        const auto duration = run_time_limit_.value().duration;
        switch (run_time_limit_.value().unit)
        {
            case score::mw::com::test::DurationUnit::sample_count:
            {
                return number_of_samples_received >= duration;
            }
            break;
            case score::mw::com::test::DurationUnit::ms:
            {
                const auto time_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
                return time_duration.count() >= duration;
            }
            break;
            case score::mw::com::test::DurationUnit::s:
            {
                const auto time_duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_);
                return time_duration.count() >= duration;
            }
            break;
            default:
            {
                score::mw::log::LogError(kLogContext) << "unknown duration type provided. Stopping the client.";
                return true;
            }
        }
    }
};

bool ReceiveEvent(TestDataProxy& lola_proxy,
                  const ClientConfig& config,
                  score::cpp::stop_token test_stop_token,
                  const RunDurationHandler& rdh)
{
    score::mw::log::LogInfo(kLogContext) << "read_cycle_time_ms == 0 -> Registering EventReceiveHandler.";
    ReceiveHandlerContext receive_handler_context{config};
    auto set_receive_handler_result =
        lola_proxy.test_event.SetReceiveHandler([&lola_proxy, &receive_handler_context, &rdh]() {
            if (receive_handler_context.reception_state != ReceptionState::RUNNING)
            {
                return;
            }
            auto number_of_callbacks_called_result = lola_proxy.test_event.GetNewSamples(
                [](SamplePtr<DataType> /*sample*/) noexcept {
                    // we receive the sample and do nothing with it.
                    // We could come up with a check to validate at least the size of the sample here.
                },
                receive_handler_context.config.max_num_samples);

            if (!number_of_callbacks_called_result.has_value())
            {
                score::mw::log::LogError() << "Client: " << number_of_callbacks_called_result.error();
                receive_handler_context.reception_state = ReceptionState::FINISHED_ERROR;
                receive_handler_context.cv.notify_one();
                return;
            }

            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(number_of_callbacks_called_result.value() > 0);

            receive_handler_context.number_of_samples_received += number_of_callbacks_called_result.value();

            if (rdh.run_duration_was_exceeded(receive_handler_context.number_of_samples_received))
            {
                receive_handler_context.reception_state = ReceptionState::FINISHED_SUCCESS;
                receive_handler_context.cv.notify_one();
                return;
            }
        });

    if (!set_receive_handler_result.has_value())
    {
        score::mw::log::LogError(kLogContext)
            << "Registering EventReceiveHandler failed: " << set_receive_handler_result.error();
        return false;
    }

    // stop_callback, which sets reception state to ABORTED and notifies waiting thread.
    score::cpp::stop_callback stop_callback{test_stop_token, [&receive_handler_context]() {
                                                {
                                                    std::unique_lock lk(receive_handler_context.mutex);
                                                    receive_handler_context.reception_state = ReceptionState::ABORTED;
                                                }
                                                receive_handler_context.cv.notify_one();
                                            }};

    // wait until receive-handler notifies, that it is done (successfully or not) with sample reception or until
    // we get stopped by stop_token.
    std::unique_lock lk(receive_handler_context.mutex);
    receive_handler_context.cv.wait(lk, [&receive_handler_context] {
        return receive_handler_context.reception_state != ReceptionState::RUNNING;
    });
    return receive_handler_context.reception_state != ReceptionState::FINISHED_ERROR;
}

bool PollForEvent(TestDataProxy& lola_proxy,
                  const ClientConfig& config,
                  score::cpp::stop_token test_stop_token,
                  const RunDurationHandler& rdh)
{
    score::mw::log::LogInfo(kLogContext) << "Entering the  GetNewSamples poll loop.";
    unsigned long number_of_samples_received{0U};
    while (!test_stop_token.stop_requested())
    {
        auto num_available_samples_result = lola_proxy.test_event.GetNumNewSamplesAvailable();
        if (!num_available_samples_result.has_value())
        {
            log::LogError() << num_available_samples_result.error();
            return false;
        }

        if (num_available_samples_result.value() == 0)
        {
            continue;
        }

        auto number_of_callbacks_called_result = lola_proxy.test_event.GetNewSamples(
            [](SamplePtr<DataType> /*sample*/) noexcept {
                // we receive the sample and do nothing with it.
                // We could come up with a check to validate at least the size of the sample here.
            },
            config.max_num_samples);

        if (!number_of_callbacks_called_result.has_value())
        {
            score::mw::log::LogError(kLogContext)
                << "Call to GetNewSamples failed: " << number_of_callbacks_called_result.error();
            return false;
        }

        // This assertion here is valid as we would have prematurely continued the loop, if
        // GetNumNewSamplesAvailable() would have returned 0! So we can expect here, that at least one callback has
        // been triggered (note, there could in-theory be race-conditions, between
        // GetNumNewSamplesAvailable/GetNewSamples, so that GetNewSamples does NOT lead to a callback, although
        // GetNumNewSamplesAvailable reports one available sample ... but this should not happen in our setup.
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(number_of_callbacks_called_result.value() > 0,
                                                    "At least one callback from GetNewSamples expected!");

        number_of_samples_received += number_of_callbacks_called_result.value();

        if (rdh.run_duration_was_exceeded(number_of_samples_received))
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{config.read_cycle_time_ms});
    }
    return true;
}

bool RunClient(const ClientConfig& config, score::cpp::stop_token test_stop_token)
{

    auto instance_specifier = InstanceSpecifier::Create(std::string{kLoLaBenchmarkInstanceSpecifier}).value();
    score::mw::log::LogInfo(kLogContext) << "Starting a Client thread";

    std::optional<TestDataProxy::HandleType> find_service_handle_result;

    ServiceFinder service_finder(config.service_finder_mode, test_stop_token);

    score::mw::log::LogInfo(kLogContext) << "Proxy handle has been found.";

    auto handle_opt = service_finder.find(instance_specifier);
    if (!handle_opt.has_value())
    {
        return false;
    }
    auto proxy_handle = handle_opt.value();

    auto lola_proxy_result = TestDataProxy::Create(proxy_handle);

    if (!lola_proxy_result.has_value())
    {
        mw::log::LogError(kLogContext) << "Could not create a proxy. Error:" << lola_proxy_result.error();
        return false;
    }

    auto& lola_proxy = lola_proxy_result.value();

    score::mw::log::LogInfo(kLogContext) << "Proxy has been Created.";

    auto subscription_result = lola_proxy.test_event.Subscribe(config.max_num_samples);

    if (!subscription_result.has_value())
    {
        mw::log::LogError(kLogContext) << "Could not subscribe to the test_event: " << subscription_result.error();
        return false;
    }

    score::mw::log::LogInfo(kLogContext) << "Subscribed to the test event.";

    RunDurationHandler rdh(config);
    if (config.read_cycle_time_ms == 0)
    {
        return ReceiveEvent(lola_proxy, config, test_stop_token, rdh);
    }
    else
    {
        return PollForEvent(lola_proxy, config, test_stop_token, rdh);
    }

    lola_proxy.test_event.Unsubscribe();
    score::mw::log::LogInfo(kLogContext) << "Unsubscribed from test_event.";

    return true;
}

}  // namespace

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    score::mw::log::LogInfo(kLogContext) << "Client Starting ...";

    auto args_result = score::mw::com::test::ParseCommandLineArgs(argc, argv, kLogContext);

    if (!args_result.has_value())
    {
        score::mw::log::LogError(kLogContext) << "Could not read command line arguments.";
        return EXIT_FAILURE;
    }
    auto args = args_result.value();

    score::cpp::stop_source test_stop_source{};

    if (!score::mw::com::test::GetStopTokenAndSetUpSigTermHandler(test_stop_source))
    {
        return EXIT_FAILURE;
    }

    auto config = score::mw::com::test::ParseClientConfig(args.config_path, kLogContext);

    score::mw::com::test::InitializeRuntime(args.service_instance_manifest);

    std::vector<std::thread> workers;
    int exit_code = EXIT_SUCCESS;

    auto test_stop_token = test_stop_source.get_token();
    for (unsigned int i = 0; i < config.number_of_clients; i++)
    {
        // fuzz the creation time of the proxies
        std::this_thread::sleep_for(std::chrono::milliseconds{std::rand() % 100});
        workers.push_back(std::thread([&config, &exit_code, &test_stop_token]() noexcept {
            auto success = score::mw::com::test::RunClient(config, test_stop_token);

            success &= score::mw::com::test::signal_service_that_client_is_done();

            if (!success)
            {
                exit_code = EXIT_FAILURE;
            }
        }));
    }
    std::for_each(workers.begin(), workers.end(), [](std::thread& t) {
        t.join();
    });

    if (exit_code == EXIT_SUCCESS)
    {
        score::mw::com::test::test_success("Client was successful.", kLogContext);
    }
    else
    {
        score::mw::com::test::test_failure("At least one of the clients failed.", kLogContext);
    }
}
