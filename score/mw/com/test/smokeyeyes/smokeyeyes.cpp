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

#include "score/mw/com/test/smokeyeyes/smokeyeyes.h"

#include "score/mw/com/runtime.h"

#include <boost/functional/hash.hpp>
#include <boost/interprocess/anonymous_shared_memory.hpp>
#include <boost/program_options.hpp>

#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

// uid 1300 - 1311 is reserved for use. See broken_link_cf/display/ipnext/User+Management
const uid_t kUidStart{1300};

namespace score::mw::com::test
{

namespace
{

class Data
{
  public:
    Data() : Data(0U) {}

    explicit Data(const std::uint32_t start_counter)
        : Data(start_counter, std::default_random_engine(std::random_device{}()))
    {
    }

    template <typename RandomNumberGenerator>
    Data(std::uint32_t start_counter, RandomNumberGenerator&& random_number_generator)
        : sequence_counter_{start_counter},
          salt_{std::uniform_int_distribution<decltype(salt_)>()(random_number_generator)},
          hash_{0U}
    {
        UpdateHash();
    }

    Data& operator++() noexcept
    {
        ++sequence_counter_;
        UpdateHash();

        return *this;
    }

    bool CheckHash() const noexcept
    {
        std::size_t hash{GenerateHash()};
        return hash == hash_;
    }

    std::uint32_t GetSequenceCounter() const noexcept
    {
        return sequence_counter_;
    }

    using IsEnumerableTag = void;

    template <typename Visitor>
    static constexpr void EnumerateTypes(Visitor& visitor) noexcept
    {
        visitor.template Visit<decltype(sequence_counter_)>();
        visitor.template Visit<decltype(salt_)>();
        visitor.template Visit<decltype(hash_)>();
    }

  private:
    void UpdateHash() noexcept
    {
        hash_ = GenerateHash();
    }

    std::size_t GenerateHash() const noexcept
    {
        std::size_t hash = 0U;
        boost::hash_combine(hash, sequence_counter_);
        boost::hash_combine(hash, salt_);
        return hash;
    }

    std::uint32_t sequence_counter_;
    std::uint32_t salt_;
    std::size_t hash_;
};

template <typename Trait>
class DataInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<Data> struct_event_{*this, "small_but_great"};
};

using DataSkeleton = AsSkeleton<DataInterface>;
using DataProxy = AsProxy<DataInterface>;

struct SharedState
{
    SharedState(const std::size_t num_clients, std::size_t stop_after) : stop_after_{stop_after}
    {
        pthread_barrierattr_t barrier_attr{};
        pthread_barrierattr_init(&barrier_attr);
        pthread_barrierattr_setpshared(&barrier_attr, PTHREAD_PROCESS_SHARED);

        pthread_barrier_init(&barrier_, &barrier_attr, static_cast<unsigned int>(num_clients + 1U));
        pthread_barrierattr_destroy(&barrier_attr);
    }

    ~SharedState()
    {
        pthread_barrier_destroy(&barrier_);
    }

    pthread_barrier_t barrier_{};
    std::atomic_size_t stop_after_{0U};
};

enum class DataState
{
    kUnchanged,
    kMessageLoss,
    kDataCorruption,
};

int run_sender(SharedState& shared_state, const std::size_t turns, const std::size_t batch_size, const bool no_wait)
{
    auto instance_specifier_result = score::mw::com::InstanceSpecifier::Create(std::string{"smokeyeyes/small_but_great"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Could not create instance specifier due to error " << std::move(instance_specifier_result).error()
                  << ", terminating!\n";
        return -4;
    }

    auto sender_result = DataSkeleton::Create(std::move(instance_specifier_result).value());
    if (!sender_result.has_value())
    {
        std::cerr << "Unable to construct sender: " << std::move(sender_result).error() << "!\n";
        return -3;
    }
    auto& sender = sender_result.value();
    const auto offer_service_result = sender.OfferService();
    if (!offer_service_result.has_value())
    {
        std::cerr << "Unable to offer service: " << offer_service_result.error() << "!\n";
        return -5;
    }

    const auto start_clock = std::chrono::steady_clock::now();
    auto time_last_stats = start_clock;

    Data data{0U};

    for (std::size_t turn = 0U; turn < turns; ++turn)
    {
        for (std::size_t sample_in_batch = 0U; sample_in_batch < batch_size; ++sample_in_batch)
        {
            sender.struct_event_.Send(data);
            ++data;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - time_last_stats > 1s)
        {
            std::cout << "\rSent samples: " << std::setfill(' ') << std::setw(16) << turn * batch_size << std::flush;
            time_last_stats = now;
        }

        if (!no_wait)
        {
            pthread_barrier_wait(&shared_state.barrier_);
        }
    }

    std::cout << "\nStopping sender\n";
    sender.StopOfferService();

    const auto duration = std::chrono::steady_clock::now() - start_clock;
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    std::cout << "Sending " << turns * batch_size << " messages took " << elapsed_ms << "ms ("
              << turns * batch_size * 1000 / static_cast<std::size_t>(elapsed_ms) << " msg/s)\n";

    return 0;
}

int run_receiver(SharedState& shared_state,
                 const std::size_t num_slots,
                 const std::size_t batch_size,
                 const bool no_wait,
                 std::ostream& out)
{
    constexpr auto FIND_SERVICE_POLL_INTERVAL{20ms};
    constexpr std::size_t FIND_SERVICE_MAX_NUM_RETRIES{250U};

    ServiceHandleContainer<DataProxy::HandleType> handles{};
    for (std::size_t retry = 0U; retry < FIND_SERVICE_MAX_NUM_RETRIES; ++retry)
    {
        // We wait before checking for the service presence to increase the possibility of finding a fully populated
        // service. Can be changed once TicketOld-81775 is implemented.
        std::this_thread::sleep_for(FIND_SERVICE_POLL_INTERVAL);
        auto instance_specifier_result =
            score::mw::com::InstanceSpecifier::Create(std::string{"smokeyeyes/small_but_great"});
        if (!instance_specifier_result.has_value())
        {
            std::cerr << "Could not create instance specifier due to error "
                      << std::move(instance_specifier_result).error() << ", terminating!\n";
            return -4;
        }

        std::promise<std::vector<DataProxy::HandleType>> service_discovery_promise{};
        auto service_discovery_future = service_discovery_promise.get_future();
        auto handles_result = DataProxy::StartFindService(
            [moved_service_discovery_promise = std::move(service_discovery_promise)](auto handles, auto handle) mutable {
                moved_service_discovery_promise.set_value(handles);
                DataProxy::StopFindService(handle);
            },
            std::move(instance_specifier_result).value());
        if (!handles_result.has_value())
        {
            std::cerr << "FindService returned an error, terminating!\n";
            return -4;
        }
        handles = std::move(service_discovery_future.get());
        if (handles.empty())
        {
            // If we didn't find a service yet (because the sender is still busy spawning clients), we back off
            // for some time. We currently cannot use StartFindService as it isn't implemented yet in Lola.
            std::this_thread::sleep_for(FIND_SERVICE_POLL_INTERVAL);
        }
        else
        {
            break;
        }
    }

    if (handles.empty())
    {
        std::cerr << "No sender instances found, terminating!\n";
        return -4;
    }

    auto receiver_result = DataProxy::Create(handles.front());
    if (!receiver_result.has_value())
    {
        std::cerr << "Unable to establish connection to sender: " << std::move(receiver_result).error()
                  << ", terminating\n";
        return -5;
    }
    auto& receiver = receiver_result.value();
    receiver.struct_event_.Subscribe(num_slots);
    for (std::size_t retry = 0U; retry < 100U; ++retry)
    {
        if (receiver.struct_event_.GetSubscriptionState() != SubscriptionState::kSubscribed)
        {
            std::this_thread::sleep_for(10ms);
        }
        else
        {
            out << "Subscribed after " << retry * 10 << "ms\n";
            break;
        }
    }

    if (receiver.struct_event_.GetSubscriptionState() != SubscriptionState::kSubscribed)
    {
        std::cerr << "PID " << getpid() << " unable to subscribe to service, terminating!\n";
        return -7;
    }

    int result{0};

    std::size_t turn{0U};
    DataState data_state{DataState::kUnchanged};
    std::size_t next_expected_sq{0U};
    while (next_expected_sq < shared_state.stop_after_.load() && data_state == DataState::kUnchanged)
    {
        out << "Turn " << turn << std::endl;
        std::size_t samples_received_in_batch{0U};
        while (samples_received_in_batch < batch_size && next_expected_sq < shared_state.stop_after_.load() &&
               data_state == DataState::kUnchanged)
        {
            auto num_samples_received = receiver.struct_event_.GetNewSamples(
                [&](SamplePtr<Data> sample) noexcept {
                    if (!sample->CheckHash())
                    {
                        data_state = DataState::kDataCorruption;
                    }

                    if (sample->GetSequenceCounter() != next_expected_sq)
                    {
                        data_state = DataState::kMessageLoss;
                    }

                    next_expected_sq = sample->GetSequenceCounter() + 1U;
                },
                batch_size - samples_received_in_batch);

            if (!num_samples_received.has_value())
            {
                out << "Error receiving sample: " << std::move(num_samples_received).error() << ", terminating!\n";
                result = -6;
                break;
            }
            else
            {
                samples_received_in_batch += *num_samples_received;
            }

            switch (data_state)
            {
                case DataState::kUnchanged:
                    break;

                case DataState::kMessageLoss:
                    out << "Detected message loss!\n";
                    break;

                case DataState::kDataCorruption:
                    out << "Detected data corruption\n";
                    break;

                default:
                    out << "Unknown DataState, terminating\n";
                    std::terminate();
                    break;
            }
        }

        if (!no_wait)
        {
            pthread_barrier_wait(&shared_state.barrier_);
        }
        ++turn;
    }

    out << "PID " << getpid() << " stopped\n";

    receiver.struct_event_.Unsubscribe();

    if (result == 0 && data_state != DataState::kUnchanged)
    {
        result = -8;
    }

    return result;
}

}  // namespace
}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    namespace po = boost::program_options;
    namespace ipc = boost::interprocess;
    using namespace score;

    std::size_t num_clients;
    std::size_t turns;
    std::size_t batch_size;
    bool no_wait;
    std::size_t num_slots;

    po::options_description options;
    // clang-format off
    options.add_options()
        ("help", "Display the help message")
        ("service_instance_manifest", po::value<std::string>(), "Path to the com configuration file")
        ("num-clients", po::value<std::size_t>(&num_clients)->default_value(1U), "Number of clients that will be spawned during a test run")
        ("turns", po::value<std::size_t>(&turns)->default_value(10U), "Number of sample batches to send before terminating")
        ("batch-size", po::value<std::size_t>(&batch_size)->default_value(3U), "Number of samples per batch")
        ("no-wait", po::bool_switch(&no_wait), "If set, do not wait for receivers after having sent a batch")
        ("log-prefix", po::value<std::string>(), "Log output to a file with this prefix and a .log suffix")
        ("num-slots", po::value<std::size_t>(&num_slots)->default_value(1U), "Number of receiver slots each client should announce during subscription");
    // clang-format on
    po::variables_map args;
    const auto parsed_args =
        po::command_line_parser{argc, argv}
            .options(options)
            .style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
            .run();
    po::store(parsed_args, args);

    if (args.count("help") > 0U)
    {
        std::cerr << options << std::endl;
        return -1;
    }

    po::notify(args);

    ipc::mapped_region shared_state_mem{ipc::anonymous_shared_memory(sizeof(mw::com::test::SharedState))};
    // We use a raw pointer here as we need to take care to only clean it up once the sender is about to terminate,
    // after having joined with all children.
    auto* shared_state =
        new (shared_state_mem.get_address()) mw::com::test::SharedState{num_clients, turns * batch_size - 1};

    std::vector<pid_t> children{};

    bool is_child = false;
    uid_t uid{kUidStart};
    for (std::size_t child = 0U; child < num_clients && !is_child; ++child)
    {
        pid_t fork_pid = fork();
        switch (fork_pid)
        {
            case -1:
                std::cerr << "Error forking child process: " << strerror(errno) << ", terminating.";
                return -1;

            case 0:
                is_child = true;
                uid += static_cast<uid_t>(child);
                break;

            default:
                std::cout << "New child PID " << fork_pid << std::endl;
                children.reserve(num_clients);
                children.push_back(fork_pid);
                break;
        }
    }

    // Has to be done after forking as messaging permanently stores the pid as the node identifier
    if (args.count("service_instance_manifest") > 0U)
    {
        mw::com::runtime::InitializeRuntime(argc, argv);
    }

    int result{};
    if (is_child)
    {
        std::reference_wrapper<std::ostream> out{std::cout};
        std::ofstream outfile{};
        if (args.count("log-prefix") > 0)
        {
            std::stringstream ss{};
            ss << args["log-prefix"].as<std::string>() << "_child_" << getpid() << ".log";
            outfile.open(ss.str().c_str(), std::ios::out | std::ios::trunc);
            out = outfile;
        }
        const auto ret_setuid = setuid(uid);
        if (ret_setuid != 0)
        {
            out.get() << "set uid fails: " << std::strerror(errno) << "\n";
            return -1;
        }
        result = mw::com::test::run_receiver(*shared_state, num_slots, batch_size, no_wait, out.get());
    }
    else
    {
        std::cout << "Running Sender ...";
        result = mw::com::test::run_sender(*shared_state, turns, batch_size, no_wait);
        std::cout << "Sender is done.";
        bool children_successful = true;
        for (auto pid : children)
        {
            int wstatus;
            waitpid(pid, &wstatus, 0);
            if (WIFEXITED(wstatus))
            {
                const auto child_status = WEXITSTATUS(wstatus);
                if (child_status != 0)
                {
                    std::cerr << "Child " << pid << " exited with " << child_status << std::endl;
                    children_successful = false;
                }
            }
        }

        shared_state->~SharedState();

        if (result == 0 && !children_successful)
        {
            result = -2;
        }
    }

    return result;
}
