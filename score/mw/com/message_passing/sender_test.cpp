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
#include "score/mw/com/message_passing/sender.h"
#include "score/mw/com/message_passing/sender_traits_mock.h"

#include <gtest/gtest.h>

#include <score/memory.hpp>

#include <unistd.h>
#include <stdio.h>
#include <cstdio>

namespace score::mw::com::message_passing
{
namespace
{

using namespace ::std::chrono_literals;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;

const std::string_view kSomeValidPath = "foo";
constexpr auto kSomeValidFileDescriptor = 1;
constexpr auto kMaxNumberOfRetries = 5;
constexpr auto kRetryDelay = 0ms;

/// \brief Grabs logs from callback passed to \c Sender
/// \attention As far as \c score::cpp::callback has passing memory limitations the static members are used here
class CallbackLogGrabber
{
  public:
    CallbackLogGrabber() noexcept
    {
        output_.clear();
        called_ = false;
    }

    void operator()(const LogFunction& func) noexcept
    {
        called_ = true;
        std::stringstream ss;
        func(ss);
        output_.append(ss.str());
    }

    bool Called() const noexcept
    {
        return called_;
    }

    std::string Output() const
    {
        return output_;
    }

  private:
    static bool called_;
    static std::string output_;
};

bool CallbackLogGrabber::called_ = false;
std::string CallbackLogGrabber::output_;

class SenderFixture : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ForwardingSenderChannelTraits::impl_ = &mock_;
    }
    void TearDown() override
    {
        ForwardingSenderChannelTraits::impl_ = nullptr;
    }

    /// \brief Instructs channel mock to succeed on any "try_open" and expect any number of (final) "close_sender"
    ///        calls.
    void PrepareChannel(const std::string_view identifier = kSomeValidPath)
    {
        EXPECT_CALL(mock_, try_open(identifier, _)).WillRepeatedly(Return(kSomeValidFileDescriptor));
        EXPECT_CALL(mock_, close_sender(kSomeValidFileDescriptor, _)).Times(AnyNumber());
    }

    /// \brief Creates sender unit with predefined config to test
    /// \param send_retry_delay Delay before retrying to send a message (default: 0 ms)
    auto CreateSender(std::chrono::milliseconds send_retry_delay = kRetryDelay)
    {
        PrepareChannel();
        // also tests SenderFactory to SenderFactoryImpl redirection
        SenderConfig sender_config{};
        sender_config.max_numbers_of_retry = kMaxNumberOfRetries;
        sender_config.send_retry_delay = send_retry_delay;
        return SenderFactoryImplMock::Create(kSomeValidPath, lifecycle_source_.get_token(), sender_config, log_clb_);
    }

    /// \brief Sets testing expectations for channel mock and predefines its behavior
    void ChannelDoesNotExistAndStopRequested()
    {
        constexpr auto kRequestStopAfterRetries = 3;  // the number does not matter
        auto counter = 0;
        EXPECT_CALL(mock_, try_open(kSomeValidPath, _))
            .WillRepeatedly(
                Invoke([counter, this](auto, auto) mutable
                           -> score::cpp::expected<ForwardingSenderChannelTraits::file_descriptor_type, score::os::Error> {
                    if (counter < kRequestStopAfterRetries)
                    {
                        ++counter;
                        lifecycle_source_.request_stop();
                    }
                    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT));
                }));
    }

    SenderChannelTraitsMock mock_{};
    score::cpp::stop_source lifecycle_source_{};
    CallbackLogGrabber log_clb_{};
};

TEST_F(SenderFixture, WillCreateUnitIfMessageChannelIsFound)
{
    // Given that the Sender calls predefined callback
    PrepareChannel();

    CallbackLogGrabber log_clb;
    score::cpp::ignore = SenderFactoryImplMock::Create(kSomeValidPath, lifecycle_source_.get_token(), {}, log_clb);

    // expect that callback log was not called.
    ASSERT_FALSE(log_clb.Called());
}

TEST_F(SenderFixture, WillWaitUntilChannelIsFound)
{
    // Given that the channel does not yet exist
    constexpr auto file_exists_after_retries = 3;  // the number does not matter
    int counter = 0;
    // Expect that try_open fails with error EOF three times, afterwards returns valid fd
    EXPECT_CALL(mock_, try_open(kSomeValidPath, _))
        .WillRepeatedly(Invoke(
            [&counter](auto,
                       auto) -> score::cpp::expected<ForwardingSenderChannelTraits::file_descriptor_type, score::os::Error> {
                if (counter < file_exists_after_retries)
                {
                    ++counter;
                    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT));
                }
                return kSomeValidFileDescriptor;
            }));
    EXPECT_CALL(mock_, close_sender);

    CallbackLogGrabber log_clb;

    // When construction the Sender
    // That the sender will wait to open the channel until it is available
    score::cpp::ignore = SenderFactoryImplMock::Create(kSomeValidPath, lifecycle_source_.get_token(), {}, log_clb);

    // Expect that callback was called and its output logs contain expected_string
    const std::string expected_str = "Could not open channel";
    const std::string output = log_clb.Output();

    ASSERT_TRUE(log_clb.Called());
    EXPECT_NE(output.find(expected_str), std::string::npos)
        << "Got output log '" << output << "', expected: '" << expected_str << "'";
    // ... and expect, that it is only contained once
    EXPECT_EQ(output.find(expected_str), output.rfind(expected_str));
    // ... and expect, that callback log contains "channel finally opened".
    constexpr auto expected_output = "channel finally opened";
    EXPECT_NE(log_clb.Output().find(expected_output), std::string::npos)
        << "output log doesn't contain '" << expected_output << "'";
}

TEST_F(SenderFixture, WillAbortWaitWhenStopRequested)
{
    // Given that the channel does not exist and a stop is requested before it exists
    ChannelDoesNotExistAndStopRequested();

    CallbackLogGrabber log_clb;

    // When construction the Sender
    // That the sender will pretend to open the respective channel
    score::cpp::ignore = SenderFactoryImplMock::Create(kSomeValidPath, lifecycle_source_.get_token(), {}, log_clb);

    // Expect that callback was called and its output logs contain expected_string
    const std::string expected_str = "Could not open channel";
    const std::string output = log_clb.Output();

    ASSERT_TRUE(log_clb.Called());
    EXPECT_NE(output.find(expected_str), std::string::npos)
        << "Got output log '" << output << "', expected: '" << expected_str << "'";
    // ... and expect, that it is only contained once
    EXPECT_EQ(output.find(expected_str), output.rfind(expected_str));
    // ... and expect, that callback log does not contains "channel finally opened".
    constexpr auto not_expected_output = "channel finally opened";
    EXPECT_EQ(log_clb.Output().find(not_expected_output), std::string::npos)
        << "output log contains '" << not_expected_output << "'";
}

TEST_F(SenderFixture, NothingSendIfNoChannelOpend)
{
    // Given that the Sender did not open the channel until stop requested
    ChannelDoesNotExistAndStopRequested();
    auto unit = SenderFactoryImplMock::Create(kSomeValidPath, lifecycle_source_.get_token());

    // Expect that nothing is send
    EXPECT_CALL(mock_, try_send(_, _, _)).Times(0);

    // And that error is reported when trying to send a message
    EXPECT_FALSE(unit->Send(ShortMessage{}));
    EXPECT_FALSE(unit->Send(MediumMessage{}));
}

TEST_F(SenderFixture, HasNonBlockingGuarantee)
{
    // Given a connected Sender
    auto unit = CreateSender();

    EXPECT_CALL(mock_, has_non_blocking_guarantee).WillOnce(Return(true));
    // Expect that the (unit test) default implementation of the Sender has non-blocking guarantee
    EXPECT_TRUE(unit->HasNonBlockingGuarantee());

    // expect that callback log was not called
    ASSERT_FALSE(log_clb_.Called());
}

TEST_F(SenderFixture, SendMessageOnFirstTry)
{
    // Given a connected Sender
    auto unit = CreateSender();

    // Expect that a message is send
    EXPECT_CALL(mock_, try_send(_, _, _)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
    EXPECT_CALL(mock_, prepare_payload(::testing::An<const ShortMessage&>()));

    // When trying to send a message
    ShortMessage message{};
    unit->Send(message);

    // expect that callback log was not called
    ASSERT_FALSE(log_clb_.Called());
}

TEST_F(SenderFixture, SendMediumMessageOnFirstTry)
{
    // Given a connected Sender
    auto unit = CreateSender();

    // Expect that a message is send
    EXPECT_CALL(mock_, try_send(_, _, _)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
    EXPECT_CALL(mock_, prepare_payload(::testing::An<const MediumMessage&>()));

    // When trying to send a medium message
    MediumMessage message{};
    unit->Send(message);

    // expect that callback log was not called
    ASSERT_FALSE(log_clb_.Called());
}

TEST_F(SenderFixture, RetryToSendMessage)
{
    // Given a connected Sender
    auto unit = CreateSender();

    // Expect that a message is send after 3 tries
    constexpr auto kSendSuccesfulAfterRetry = 3;
    auto invoke_counter = 0;
    EXPECT_CALL(mock_, try_send(_, _, _))
        .WillRepeatedly(Invoke([&invoke_counter](auto, auto, auto) -> score::cpp::expected_blank<score::os::Error> {
            if (invoke_counter < kSendSuccesfulAfterRetry)
            {
                ++invoke_counter;
                return score::cpp::make_unexpected(score::os::Error::createFromErrno(EOF));
            }
            return score::cpp::expected_blank<score::os::Error>{};
        }));
    EXPECT_CALL(mock_, prepare_payload(::testing::An<const ShortMessage&>()));

    // When trying to send a message
    ShortMessage message{};
    const auto result = unit->Send(message);

    // That only 3 tries are used
    EXPECT_EQ(invoke_counter, kSendSuccesfulAfterRetry);
    EXPECT_TRUE(result.has_value());

    // expect that callback log was not called
    ASSERT_FALSE(log_clb_.Called());
}

TEST_F(SenderFixture, StopRetryAfter5Times)
{
    // Given a connected Sender
    auto unit = CreateSender();

    // Expect that a message is not send
    auto invoke_counter = 0;
    EXPECT_CALL(mock_, try_send(_, _, _))
        .WillRepeatedly(Invoke([&invoke_counter](auto, auto, auto) -> score::cpp::expected_blank<score::os::Error> {
            ++invoke_counter;
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EOF));
        }));
    EXPECT_CALL(mock_, prepare_payload(::testing::An<const ShortMessage&>()));

    // When trying to send a message
    ShortMessage message{};
    const auto result = unit->Send(message);

    // That only 5 tries are used
    EXPECT_EQ(invoke_counter, 5);
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(EOF));

    // expect that callback log was not called
    ASSERT_FALSE(log_clb_.Called());
}

TEST_F(SenderFixture, StopAfterStopRequest)
{
    // Given a connected Sender
    auto unit = CreateSender(kRetryDelay);

    // Expect that no more messages are sent after stop request
    auto invoke_counter = 0;
    constexpr auto number_of_send_attempts_before_stop_request = kMaxNumberOfRetries - 1;
    EXPECT_CALL(mock_, try_send(_, _, _))
        .WillRepeatedly(Invoke([&invoke_counter, this](auto, auto, auto) -> score::cpp::expected_blank<score::os::Error> {
            ++invoke_counter;
            if (invoke_counter == number_of_send_attempts_before_stop_request)
            {
                lifecycle_source_.request_stop();
            }
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EOF));
        }));
    EXPECT_CALL(mock_, prepare_payload(::testing::An<const ShortMessage&>()));

    // When trying to send a message
    ShortMessage message{};
    const auto result = unit->Send(message);

    // That only 3 tries are used
    EXPECT_EQ(invoke_counter, number_of_send_attempts_before_stop_request);
    EXPECT_EQ(result.error(), score::os::Error::createFromErrno(EOF));

    // expect that callback log was not called
    ASSERT_FALSE(log_clb_.Called());
}

TEST_F(SenderFixture, SenderShouldRetrySendingWithDelay)
{
    // Given a connected Sender with a retry delay of 10 ms and 5 maximum number of retries.
    constexpr auto send_retry_delay = 10ms;
    auto unit = CreateSender(send_retry_delay);

    // And given that sending the message fails every time.
    EXPECT_CALL(mock_, try_send(_, _, _))
        .WillRepeatedly(Invoke([](auto, auto, auto) -> score::cpp::expected_blank<score::os::Error> {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EOF));
        }));
    EXPECT_CALL(mock_, prepare_payload(::testing::An<const ShortMessage&>()));

    // When trying to send a message
    ShortMessage message{};
    const auto start_time = std::chrono::steady_clock::now();
    const auto result = unit->Send(message);
    const auto elapsed_time = std::chrono::steady_clock::now() - start_time;

    // Expect a delay of at least 5 * 10 ms = 50 ms.
    EXPECT_GE(elapsed_time, (send_retry_delay * kMaxNumberOfRetries));

    // expect that callback log was not called
    ASSERT_FALSE(log_clb_.Called());
}

}  // namespace
}  // namespace score::mw::com::message_passing
