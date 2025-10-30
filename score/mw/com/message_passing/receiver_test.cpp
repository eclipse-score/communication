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
#include "score/mw/com/message_passing/receiver_factory_impl.h"

#include "score/concurrency/thread_pool.h"
#include "score/mw/com/message_passing/receiver_traits_mock.h"
#include "score/mw/com/message_passing/serializer.h"

#include "score/mw/com/message_passing/receiver.h"

#include <future>
#include <thread>

#include <gtest/gtest.h>

namespace score::mw::com::message_passing
{
namespace
{

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

constexpr auto SOME_PATH = "/foo";
constexpr auto VALID_FILE_DESCRIPTOR = 1;
constexpr std::array<uint8_t, 16>
    MEDIUM_MSG_PAYLOAD{'h', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', '!', '!', '!', '!'};

class ReceiverFixture : public ::testing::Test
{
  public:
    using file_descriptor_type = ForwardingReceiverChannelTraits::file_descriptor_type;
    using FileDescriptorResourcesType = ForwardingReceiverChannelTraits::FileDescriptorResourcesType;
    using ShortMessageProcessor = std::function<void(ShortMessage)>;
    using MediumMessageProcessor = std::function<void(MediumMessage)>;

    void SetUp() override
    {
        ReceiverConfig receiver_config{};
        ReceiverConfig receiver_config_without_delay{};
        receiver_config.message_loop_delay = std::chrono::milliseconds{1};

        ForwardingReceiverChannelTraits::impl_ = &mock_;
        unit_ = ReceiverFactoryMock::Create(SOME_PATH, thread_pool_, score::cpp::span<const uid_t>{}, receiver_config);
        unit_without_delay_ = ReceiverFactoryMock::Create(
            SOME_PATH, thread_pool_, score::cpp::span<const uid_t>{}, receiver_config_without_delay);
    }

    void TearDown() override
    {
        unit_.reset();
        unit_without_delay_.reset();
        ForwardingReceiverChannelTraits::impl_ = nullptr;
    }

    score::concurrency::ThreadPool thread_pool_{1};
    ReceiverChannelTraitsMock mock_{};
    score::cpp::pmr::unique_ptr<score::mw::com::message_passing::IReceiver> unit_{};
    score::cpp::pmr::unique_ptr<score::mw::com::message_passing::IReceiver> unit_without_delay_{};
};

TEST_F(ReceiverFixture, CanRegisterACallback)
{
    // Given a valid unit prepared in SetUp()
    // Expect, that registering a callback for handling received short-messages works (no exception/no termination).
    unit_->Register(0x42,
                    score::cpp::callback<void(const ShortMessagePayload, const pid_t)>{
                        [](const ShortMessagePayload, const pid_t) noexcept {}});
}

TEST_F(ReceiverFixture, CanOpenUnderlyingChannel)
{
    // Given a valid unit prepared in SetUp()
    // Expect, that "open_receiver" gets called on the channel traits
    EXPECT_CALL(mock_, open_receiver(_, _, _, _)).WillOnce(Return(VALID_FILE_DESCRIPTOR));

    // When starting to listen
    const auto result = unit_->StartListening();

    // Then no error occurs
    EXPECT_TRUE(result);

    // And "close_receiver" will be called on unit_ destruction
    EXPECT_CALL(mock_, close_receiver).Times(1);
}

TEST_F(ReceiverFixture, TriggerStopToken)
{
    // Given a valid unit prepared in SetUp()
    // Expect, that "open_receiver" gets called on the channel traits
    EXPECT_CALL(mock_, open_receiver(_, _, _, _)).WillOnce(Return(VALID_FILE_DESCRIPTOR));

    // and stop is requested
    thread_pool_.Shutdown();
    // When starting to listen
    const auto result = unit_->StartListening();

    // Then no error occurs
    EXPECT_TRUE(result);

    // And "close_receiver" will be called on unit_ destruction
    EXPECT_CALL(mock_, close_receiver).Times(1);
}

TEST_F(ReceiverFixture, CanNotOpenUnderlyingChannel)
{
    // Expect that the communication channel can not be opened
    EXPECT_CALL(mock_, open_receiver(_, _, _, _))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EOF))));

    // When starting to listen
    const auto result = unit_->StartListening();

    // Then an error occurred
    EXPECT_FALSE(result);
}

TEST_F(ReceiverFixture, DestructReceiverWhileListeningWillStop)
{
    // Given a valid path and constructed unit
    auto unit = ReceiverFactoryMock::Create(SOME_PATH, thread_pool_, score::cpp::span<const uid_t>{}, ReceiverConfig{});

    // Expect that "open_receiver" gets called on the channel traits and "close_receiver" as well
    EXPECT_CALL(mock_, open_receiver(_, _, _, _)).WillOnce(Return(VALID_FILE_DESCRIPTOR));

    // Expect that listening does not return an error
    EXPECT_TRUE(unit->StartListening());

    // And "close_receiver" will be called on unit_ destruction
    EXPECT_CALL(mock_, close_receiver).Times(1);
}

TEST_F(ReceiverFixture, CorrectCallbackIsInvokedForProperMessage)
{
    // Given a unit that has registered callbacks
    std::atomic<ShortMessagePayload> short_payload{};
    std::promise<MediumMessagePayload> medium_payload_promise{};
    // ... and has registered callbacks
    unit_->Register(0x42,
                    score::cpp::callback<void(const ShortMessagePayload, const pid_t)>{
                        [&short_payload](const ShortMessagePayload message_payload, const pid_t sender_pid) noexcept {
                            short_payload = message_payload;
                            EXPECT_EQ(sender_pid, 1233);
                        }});
    std::future<MediumMessagePayload> mediumPayloadFuture = medium_payload_promise.get_future();
    unit_->Register(
        0x50,
        score::cpp::callback<void(const MediumMessagePayload, const pid_t)>{
            [&medium_payload_promise](const MediumMessagePayload message_payload, const pid_t sender_pid) noexcept {
                medium_payload_promise.set_value(message_payload);
                EXPECT_EQ(sender_pid, 1233);
            }});

    unit_->Register(0x43,
                    score::cpp::callback<void(const ShortMessagePayload, const pid_t)>{
                        [](const ShortMessagePayload, const pid_t) noexcept {
                            ASSERT_FALSE(true);
                        }});

    // Expect call to open_receiver on underlying receiver traits
    EXPECT_CALL(mock_, open_receiver(_, _, _, _)).WillOnce(Return(VALID_FILE_DESCRIPTOR));
    // Expect call to receive_next on underlying receiver traits, which:
    //  - 1st will call medium message handler callback with a medium message with id 0x50 and payload
    //        MEDIUM_MSG_PAYLOAD and indicate, that it will wait on next message
    //  - 2nd will call short message handler callback with a short message with id 0x42 and payload 0x42 and indicate,
    //        that it will wait on next message
    //  - 3rd will indicate that it has received a stop-request and won't wait for any further message
    EXPECT_CALL(mock_, receive_next)
        .WillOnce(Invoke(
            [](const file_descriptor_type /*file_descriptor*/,
               std::size_t /*thread*/,
               ShortMessageProcessor /*fShort*/,
               MediumMessageProcessor fMedium,
               const FileDescriptorResourcesType& /*os_resources*/) -> score::cpp::expected<bool, score::os::Error> {
                MediumMessage message{};
                message.id = 0x50;
                message.pid = 1233;
                message.payload = MEDIUM_MSG_PAYLOAD;
                fMedium(message);
                return true;
            }))
        .WillOnce(Invoke(
            [](const file_descriptor_type /*file_descriptor*/,
               std::size_t /*thread*/,
               ShortMessageProcessor fShort,
               MediumMessageProcessor /*fMedium*/,
               const FileDescriptorResourcesType& /*os_resources*/) -> score::cpp::expected<bool, score::os::Error> {
                ShortMessage message{};
                message.id = 0x42;
                message.pid = 1233;
                message.payload = 0x42;
                fShort(message);
                return true;
            }))
        .WillOnce(Invoke([](const file_descriptor_type,
                            std::size_t,
                            ShortMessageProcessor,
                            MediumMessageProcessor,
                            const FileDescriptorResourcesType&) -> score::cpp::expected<bool, score::os::Error> {
            return false;
        }));

    // When starting to listen on the unit
    unit_->StartListening();

    // expect that at some point/after a while the short message has been received containing the payload 0x42
    while (short_payload != 0x42 ||
           (mediumPayloadFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
    {
        std::this_thread::yield();
    }
    // ... and expect, that the medium message has been received then as well as it was received before the short
    //     message.
    EXPECT_EQ(mediumPayloadFuture.get(), MEDIUM_MSG_PAYLOAD);

    // finally on destruction of our receiver/unit_
    // expect stop_receive and close_receiver being called on underlying receiver traits
    EXPECT_CALL(mock_, close_receiver).Times(1);
    EXPECT_CALL(mock_, stop_receive).Times(AnyNumber());
}

TEST_F(ReceiverFixture, CorrectCallbackIsInvokedForProperMessageWithoutDelay)
{
    // Given a unit that has registered callbacks
    std::atomic<ShortMessagePayload> short_payload{};
    std::promise<MediumMessagePayload> medium_payload_promise{};
    // ... and has registered callbacks
    unit_without_delay_->Register(
        0x42,
        score::cpp::callback<void(const ShortMessagePayload, const pid_t)>{
            [&short_payload](const ShortMessagePayload message_payload, const pid_t sender_pid) noexcept {
                short_payload = message_payload;
                EXPECT_EQ(sender_pid, 1233);
            }});
    std::future<MediumMessagePayload> mediumPayloadFuture = medium_payload_promise.get_future();
    unit_without_delay_->Register(
        0x50,
        score::cpp::callback<void(const MediumMessagePayload, const pid_t)>{
            [&medium_payload_promise](const MediumMessagePayload message_payload, const pid_t sender_pid) noexcept {
                medium_payload_promise.set_value(message_payload);
                EXPECT_EQ(sender_pid, 1233);
            }});

    unit_without_delay_->Register(0x43,
                                  score::cpp::callback<void(const ShortMessagePayload, const pid_t)>{
                                      [](const ShortMessagePayload, const pid_t) noexcept {
                                          ASSERT_FALSE(true);
                                      }});

    // Expect call to open_receiver on underlying receiver traits
    EXPECT_CALL(mock_, open_receiver(_, _, _, _)).WillOnce(Return(VALID_FILE_DESCRIPTOR));
    // Expect call to receive_next on underlying receiver traits, which:
    //  - 1st will call medium message handler callback with a medium message with id 0x50 and payload
    //        MEDIUM_MSG_PAYLOAD and indicate, that it will wait on next message
    //  - 2nd will call short message handler callback with a short message with id 0x42 and payload 0x42 and indicate,
    //        that it will wait on next message
    //  - 3rd will indicate that it has received a stop-request and won't wait for any further message
    EXPECT_CALL(mock_, receive_next)
        .WillOnce(Invoke(
            [](const file_descriptor_type /*file_descriptor*/,
               std::size_t /*thread*/,
               ShortMessageProcessor /*fShort*/,
               MediumMessageProcessor fMedium,
               const FileDescriptorResourcesType& /*os_resources*/) -> score::cpp::expected<bool, score::os::Error> {
                MediumMessage message{};
                message.id = 0x50;
                message.pid = 1233;
                message.payload = MEDIUM_MSG_PAYLOAD;
                fMedium(message);
                return true;
            }))
        .WillOnce(Invoke(
            [](const file_descriptor_type /*file_descriptor*/,
               std::size_t /*thread*/,
               ShortMessageProcessor fShort,
               MediumMessageProcessor /*fMedium*/,
               const FileDescriptorResourcesType& /*os_resources*/) -> score::cpp::expected<bool, score::os::Error> {
                ShortMessage message{};
                message.id = 0x42;
                message.pid = 1233;
                message.payload = 0x42;
                fShort(message);
                return true;
            }))
        .WillOnce(Invoke([](const file_descriptor_type,
                            std::size_t,
                            ShortMessageProcessor,
                            MediumMessageProcessor,
                            const FileDescriptorResourcesType&) -> score::cpp::expected<bool, score::os::Error> {
            return false;
        }));

    // When starting to listen on the unit
    unit_without_delay_->StartListening();

    // expect that at some point/after a while the short message has been received containing the payload 0x42
    while (short_payload != 0x42 ||
           (mediumPayloadFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
    {
        std::this_thread::yield();
    }
    // ... and expect, that the medium message has been received then as well as it was received before the short
    //     message.
    EXPECT_EQ(mediumPayloadFuture.get(), MEDIUM_MSG_PAYLOAD);

    // finally on destruction of our receiver/unit_without_delay_
    // expect stop_receive and close_receiver being called on underlying receiver traits
    EXPECT_CALL(mock_, close_receiver).Times(1);
    EXPECT_CALL(mock_, stop_receive).Times(AnyNumber());
}

TEST_F(ReceiverFixture, ReceiveMessageWhileNoCallbackIsRegistered)
{
    // Given a unit that has no registered callbacks
    std::atomic_bool receive_function_called{false};

    // Expect call to open_receiver on underlying receiver traits
    EXPECT_CALL(mock_, open_receiver(_, _, _, _)).WillOnce(Return(VALID_FILE_DESCRIPTOR));
    // Expect call to receive_next on underlying receiver traits, which:
    //  - 1st will call medium message handler callback with a medium message with id 0x50 and payload
    //        MEDIUM_MSG_PAYLOAD and indicate, that it will wait on next message
    //  - 2nd will call short message handler callback with a short message with id 0x42 and payload 0x42 and indicate,
    //        that it will wait on next message
    //  - 3rd will indicate that it has received a stop-request and won't wait for any further message
    EXPECT_CALL(mock_, receive_next)
        .WillOnce(Invoke(
            [](const file_descriptor_type /*file_descriptor*/,
               std::size_t /*thread*/,
               ShortMessageProcessor /*fShort*/,
               MediumMessageProcessor fMedium,
               const FileDescriptorResourcesType& /*os_resources*/) -> score::cpp::expected<bool, score::os::Error> {
                MediumMessage message{};
                message.id = 0x50;
                message.pid = 1233;
                message.payload = MEDIUM_MSG_PAYLOAD;
                fMedium(message);
                return true;
            }))
        .WillOnce(Invoke([&receive_function_called](const file_descriptor_type /*file_descriptor*/,
                                                    std::size_t /*thread*/,
                                                    ShortMessageProcessor fShort,
                                                    MediumMessageProcessor /*fMedium*/,
                                                    const FileDescriptorResourcesType& /*os_resources*/)
                             -> score::cpp::expected<bool, score::os::Error> {
            ShortMessage message{};
            message.id = 0x42;
            message.payload = 0x42;
            fShort(message);
            receive_function_called = true;
            return true;
        }))
        .WillOnce(Invoke([](const file_descriptor_type,
                            std::size_t,
                            ShortMessageProcessor,
                            MediumMessageProcessor,
                            const FileDescriptorResourcesType&) -> score::cpp::expected<bool, score::os::Error> {
            return false;
        }));

    // When starting to listen on the unit
    unit_->StartListening();

    // that nothing is happening and the message is ignored,
    // proceeds the check until the second callback is called
    while (!receive_function_called)
    {
        std::this_thread::yield();
    }

    // finally on destruction of our receiver/unit_
    // expect stop_received and close_receiver being called on underlying receiver traits
    EXPECT_CALL(mock_, close_receiver).Times(1);
    EXPECT_CALL(mock_, stop_receive).Times(AnyNumber());
}

TEST_F(ReceiverFixture, ReceivedErrorFromChannelTraits)
{
    // Given a valid unit prepared in SetUp()
    std::atomic_bool error_flag{false};
    // Expect call to open_receiver on underlying receiver traits
    EXPECT_CALL(mock_, open_receiver(_, _, _, _)).WillOnce(Return(VALID_FILE_DESCRIPTOR));
    // Expect call to receive_next on underlying receiver traits, which:
    //  - 1st will call will return EOF error
    //  - 2nd will indicate that it has received a stop-request and won't wait for any further message
    EXPECT_CALL(mock_, receive_next)
        .WillOnce(Invoke([&error_flag](const file_descriptor_type /*file_descriptor*/,
                                       std::size_t /*thread*/,
                                       ShortMessageProcessor /*fShort*/,
                                       MediumMessageProcessor /*fMedium*/,
                                       const FileDescriptorResourcesType& /*os_resources*/)
                             -> score::cpp::expected<bool, score::os::Error> {
            error_flag = true;
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EOF));
        }))
        .WillOnce(Invoke([](const file_descriptor_type,
                            std::size_t,
                            ShortMessageProcessor,
                            MediumMessageProcessor,
                            const FileDescriptorResourcesType&) -> score::cpp::expected<bool, score::os::Error> {
            return false;
        }));

    // When starting to listen on the unit
    unit_->StartListening();

    // that the payload is the expected
    while (error_flag == false)
    {
        std::this_thread::yield();
    }

    // finally on destruction of our receiver/unit_
    // expect stop_received and close_receiver being called on underlying receiver traits
    EXPECT_CALL(mock_, close_receiver).Times(1);
    EXPECT_CALL(mock_, stop_receive).Times(AnyNumber());
}

}  // namespace
}  // namespace score::mw::com::message_passing
