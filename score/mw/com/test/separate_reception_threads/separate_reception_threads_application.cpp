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

#include "score/concurrency/thread_pool.h"
#include "score/os/unistd.h"
#include "score/mw/com/impl/binding_event_receive_handler.h"
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/i_runtime.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_common.h"
#include "score/mw/com/impl/bindings/lola/messaging/messages/message_element_fq_id.h"
#include "score/mw/com/impl/i_runtime_binding.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/message_passing/i_sender.h"
#include "score/mw/com/message_passing/receiver_factory.h"
#include "score/mw/com/message_passing/sender_factory.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"

#include "platform/aas/language/safecpp/scoped_function/scope.h"
#include "platform/aas/mw/log/logging.h"

// #include <amp_assert.hpp>
#include <amp_stop_token.hpp>
#include <amp_string_view.hpp>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <thread>

using namespace bmw::mw::com;
using namespace std::chrono_literals;

const std::chrono::milliseconds sender_cycle{50};
impl::lola::ElementFqId dummy_element_fq_id{42, 1, 1, impl::lola::ElementType::EVENT};
std::atomic_bool stop_sending{false};
const pid_t kDummyTargetNodeId{222};
const char kReceiveNameQM[] = "/LoLa_222_QM";
const char kReceiveNameASIL_B[] = "/LoLa_222_ASIL_B";

void message_sender(amp::stop_token stop_token)
{
    bmw::mw::log::LogInfo("lola") << "Starting message sender ...";

    auto node_id = bmw::os::Unistd::instance().getpid();
    std::stringstream identifier;
    identifier << "/LoLa_" << node_id << "_ASIL_B";
    const std::string receiver_name_asil_b = identifier.str();
    identifier = std::stringstream();
    identifier << "/LoLa_" << node_id << "_QM";
    const std::string receiver_name_qm = identifier.str();

    auto asil_b_sender = message_passing::SenderFactory::Create(receiver_name_asil_b.data(), stop_token);
    auto qm_sender = message_passing::SenderFactory::Create(receiver_name_qm.data(), stop_token);
    impl::lola::ElementFqIdMessage<impl::lola::MessageType::kNotifyEvent> message{dummy_element_fq_id, node_id};
    message_passing::ShortMessage serialized_message = message.SerializeToShortMessage();

    while (!stop_token.stop_requested() && !stop_sending)
    {
        auto result = asil_b_sender->Send(serialized_message);
        if (result.has_value() == false)
        {
            std::cerr << "Error sending message to ASIL-B receiver!" << std::endl;
        }

        result = qm_sender->Send(serialized_message);
        if (result.has_value() == false)
        {
            std::cerr << "Error sending message to QM receiver!" << std::endl;
        }
        std::this_thread::sleep_for(sender_cycle);
    }
}

/// \brief Test application to verify that LoLa applies distinct threads to receive/process messages received from its
///        ASIL-B message_receiver and its ASIL-QM message_receiver (Requirement )
/// \details  requires, that: Each message passing port shall use a custom thread
///          The background is, that we want to assure a high availability for the ASIL-B path! If we wouldn't apply
///          different threads to ASIL-QM and ASIL-B reception paths, unsecure ASIL-QM clients could "flood" the ASIL-QM
///          receiver with messages, thereby affecting also the ASIL-B side, if the same worker thread would process
///          both receivers!
///          The test verifies the thread separation by:
///          - Registering a message handler on the QM-receiver, which blocks extremely long.
///          - Registering a well behaving handler on the ASIL-B-receiver, which counts its calls/activations
///          - applies a message send-thread, which sends alternating messages to both receivers.
///          - in the main thread the number of processed incoming messages on ASIL-B receiver is cyclically checked.
///            The overall number of cycles done here also determines the test runtime/duration.
///
///         Verification: If during cyclical evaluation it is detected, that no new incoming messages on ASIL-B receiver
///         get processed, this leads to a premature abort of the test. The final verdict, whether the test is
///         successful or not, depends on the number of received/processed incoming messages on the ASIL-B receiver:
///         Since we now the cycle time of the sender and the overall test runtime, we have an expectation how many
///         ASIL-B messages shall be processed. We subtract 10% (to compensate scheduling jitter) from the expectation,
///         but if the number of processed ASIL-B messages is lower than that, the test result is a failure.
///
/// \return EXIT_SUCCESS in case the number of processed ASIL-B messages fulfills expectation, EXIT_FAILURE otherwise.
int main(int argc, const char *argv[])
{
    namespace fs = boost::filesystem;

    using Parameters = bmw::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    bmw::mw::log::LogInfo("lola") << "Starting lola message passing app ...";

    const std::vector<Parameters> allowed_parameters{Parameters::SERVICE_INSTANCE_MANIFEST};
    bmw::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto &run_parameters = test_runner.GetRunParameters();
    const auto optional_service_instance_manifest = run_parameters.GetOptionalServiceInstanceManifest();
    const auto stop_token = test_runner.GetStopToken();

    // HAXX: Create symlink for logging.json so that it appears in the cwd
    const fs::path logging_json_target_path{fs::current_path() / "logging.json"};
    if (!fs::exists(logging_json_target_path))
    {
        const fs::path logging_json_source_path{fs::current_path() / "platform" / "aas" / "mw" / "com" / "test" /
                                                "separate_reception_threads" / "logging.json"};
        std::cout << "Symlinking logging.json from " << logging_json_source_path << " to " << logging_json_target_path
                  << std::endl;
        fs::create_symlink(logging_json_source_path, logging_json_target_path);
    }

    auto &runtime = impl::Runtime::getInstance();

    impl::IRuntimeBinding *binding_runtime = runtime.GetBindingRuntime(impl::BindingType::kLoLa);
    AMP_ASSERT_PRD_MESSAGE(binding_runtime != nullptr, "Binding runtime should not be null.");
    auto *lola_runtime = dynamic_cast<impl::lola::IRuntime *>(binding_runtime);

    // our config (mw_com_config.json is configured with "asil-level": "B")
    AMP_ASSERT_PRD_MESSAGE(lola_runtime->HasAsilBSupport(), "Config should be configured with \"asil-level\": \"B\".");

    auto &messaging = lola_runtime->GetLolaMessaging();

    // Create Receiver for QM/ASIL_B, which receive the "RegisterEventUpdateNotification" messages, which will be sent
    // during the test, when RegisterEventNotification() gets called.
    // If we don't have those receivers the whole test would block as Sender creation is blocking and
    // wouldn't succeed, if we have no corresponding receivers, which are listening.
    auto thread_pool_receiver = std::make_unique<bmw::concurrency::ThreadPool>(4, "test_receiver_threadpool");
    bmw::mw::com::message_passing::ReceiverConfig receiver_config{};
    auto receiver_qm = message_passing::ReceiverFactory::Create(
        kReceiveNameQM, *thread_pool_receiver, amp::span<const uid_t>{}, receiver_config);
    auto receiver_asilb = message_passing::ReceiverFactory::Create(
        kReceiveNameASIL_B, *thread_pool_receiver, amp::span<const uid_t>{}, receiver_config);
    auto listen_result = receiver_qm->StartListening();
    if (!listen_result.has_value())
    {
        std::cerr << "Failed to listen on QM receiver." << std::endl;
        return EXIT_FAILURE;
    }
    listen_result = receiver_asilb->StartListening();
    if (!listen_result.has_value())
    {
        std::cerr << "Failed to listen on ASIL_B receiver." << std::endl;
        return EXIT_FAILURE;
    }

    std::uint32_t asil_b_call_count{0};
    std::uint32_t asil_qm_call_count{0};

    // our "good" behaving message handler for handling event-update-notification messages on ASIL-B receiver just
    // maintains a call counter and returns immediately.
    const bmw::safecpp::Scope<> event_receive_handler_scope{};
    bmw::mw::com::impl::BindingEventReceiveHandler event_update_notification_handler_asil_b{
        event_receive_handler_scope, [&asil_b_call_count]() noexcept -> void
        { asil_b_call_count++; }};

    // our "bad" behaving message handler for handling event-update-notification messages on ASIL-QM receiver maintains
    // a call counter and sleeps for a long time.
    bmw::mw::com::impl::BindingEventReceiveHandler event_update_notification_handler_asil_qm{
        event_receive_handler_scope, [&asil_qm_call_count]() -> void
        {
            asil_qm_call_count++;
            std::uint8_t sleep_count{0};
            while (stop_sending && sleep_count < 255)
            {
                std::this_thread::sleep_for(1s);
                sleep_count++;
            }
        }};

    const auto registration_number_asil_b =
        messaging.RegisterEventNotification(impl::QualityType::kASIL_B,
                                            dummy_element_fq_id,
                                            std::move(event_update_notification_handler_asil_b),
                                            kDummyTargetNodeId);
    const auto registration_number_qm =
        messaging.RegisterEventNotification(impl::QualityType::kASIL_QM,
                                            dummy_element_fq_id,
                                            std::move(event_update_notification_handler_asil_qm),
                                            kDummyTargetNodeId);

    std::thread send_thread(message_sender, stop_token);

    unsigned counter{0}, last_count{0};
    bool asil_b_rec_stuck{false};
    while (!stop_token.stop_requested() && !asil_b_rec_stuck && counter < 100)
    {
        std::this_thread::sleep_for(2 * sender_cycle);
        counter++;
        // we verify/check the aliveness of the ASIL-B reception channel only every 5th main iteration.
        if (counter % 5 == 0)
        {
            if (asil_b_call_count == last_count)
            {
                asil_b_rec_stuck = true;
            }
            last_count = asil_b_call_count;
        }
    }
    stop_sending = true;
    send_thread.join();

    const std::uint32_t expected_min_asil_b_call_count = 2 * counter - (2 * counter / 10);

    messaging.UnregisterEventNotification(
        impl::QualityType::kASIL_B, dummy_element_fq_id, registration_number_asil_b, kDummyTargetNodeId);
    messaging.UnregisterEventNotification(
        impl::QualityType::kASIL_QM, dummy_element_fq_id, registration_number_qm, kDummyTargetNodeId);

    if (asil_b_call_count >= expected_min_asil_b_call_count)
    {
        std::cout << "Success! ASIL-B messages have been continuously received." << std::endl;
        return EXIT_SUCCESS;
    }
    else
    {
        std::cerr << "Error! ASIL-B messages haven't been continuously received! We received only " << asil_b_call_count
                  << " messages!" << std::endl;
        std::cerr << "Expected minimum of ASIL-B messages: " << expected_min_asil_b_call_count << std::endl;
        return EXIT_FAILURE;
    }
}
