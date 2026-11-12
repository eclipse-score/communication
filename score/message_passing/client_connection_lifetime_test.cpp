#include "score/message_passing/client_connection_lifetime.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>

namespace score::message_passing::detail
{
namespace
{

using State = IClientConnection::State;
using StopReason = IClientConnection::StopReason;
using Result = score::cpp::expected_blank<score::os::Error>;

constexpr std::int32_t kDescriptor{42};

class ClientConnectionLifetimeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        lifetime_.InstallDescriptor(kDescriptor);
    }

    std::atomic<State> state_{State::kReady};
    std::atomic<StopReason> reason_{StopReason::kNone};
    std::uint32_t retirement_count_{0};
    ClientConnectionLifetime lifetime_{state_, reason_, [this]() noexcept {
                                           ++retirement_count_;
                                       }};
};

TEST_F(ClientConnectionLifetimeTest, NonReadyStatesRejectTransactionsWithoutInvokingActor)
{
    // Given a descriptor whose connection is not ready
    for (const State state : {State::kStarting, State::kStopping, State::kStopped})
    {
        state_ = state;
        std::uint32_t actor_count{0};

        // When a transaction is requested
        const auto result = lifetime_.ExecuteTransaction([&actor_count](auto) {
            ++actor_count;
            return Result{};
        });

        // Then admission fails without invoking the actor or scheduling retirement
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().GetOsDependentErrorCode(), EINVAL);
        EXPECT_FALSE(lifetime_.IsAccepting());
        EXPECT_EQ(actor_count, 0U);
        EXPECT_EQ(retirement_count_, 0U);
    }
}

TEST_F(ClientConnectionLifetimeTest, SelectedStopReasonRejectsTransactionEvenWhileStateIsReady)
{
    // Given a ready state with a stop reason already selected
    ASSERT_TRUE(lifetime_.TrySetStopReason(StopReason::kUserRequested));

    // When another transaction is requested
    const auto result = lifetime_.ExecuteTransaction([](auto) {
        ADD_FAILURE() << "the actor must not run after stopping is selected";
        return Result{};
    });

    // Then the selected reason closes admission before state publication
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), EINVAL);
    EXPECT_EQ(state_, State::kReady);
    EXPECT_FALSE(lifetime_.IsAccepting());
}

TEST_F(ClientConnectionLifetimeTest, FirstStopReasonIsPreserved)
{
    // Given an accepting connection
    ASSERT_TRUE(lifetime_.IsAccepting());

    // When two callers try selecting different stop reasons
    const bool first = lifetime_.TrySetStopReason(StopReason::kUserRequested);
    const bool second = lifetime_.TrySetStopReason(StopReason::kIoError);

    // Then only the first caller owns stopping and its reason is preserved
    EXPECT_TRUE(first);
    EXPECT_FALSE(second);
    EXPECT_EQ(reason_, StopReason::kUserRequested);
    EXPECT_FALSE(lifetime_.IsAccepting());
}

TEST_F(ClientConnectionLifetimeTest, TransactionReceivesDescriptorAndReturnsActorValue)
{
    // Given a ready connection with an installed descriptor
    ASSERT_TRUE(lifetime_.IsAccepting());

    // When an admitted actor returns a value
    const auto result = lifetime_.ExecuteTransaction([](std::int32_t descriptor) {
        EXPECT_EQ(descriptor, kDescriptor);
        return score::cpp::expected<std::uint32_t, score::os::Error>{17U};
    });

    // Then the value is returned unchanged without initiating retirement
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 17U);
    EXPECT_FALSE(lifetime_.RetirementPending());
    EXPECT_EQ(retirement_count_, 0U);
}

TEST_F(ClientConnectionLifetimeTest, CancellationDuringTransactionDefersRetirementUntilReturn)
{
    // Given one admitted transaction
    const auto result = lifetime_.ExecuteTransaction([this](auto) {
        // When stopping and cancellation occur while the actor is executing
        EXPECT_TRUE(lifetime_.TrySetStopReason(StopReason::kUserRequested));
        EXPECT_FALSE(lifetime_.CancellationFinished());

        // Then the active operation prevents immediate retirement
        EXPECT_TRUE(lifetime_.RetirementPending());
        EXPECT_EQ(retirement_count_, 0U);
        return Result{};
    });

    // Then leaving the last operation requests retirement exactly once
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(retirement_count_, 1U);
    EXPECT_EQ(lifetime_.RetireDescriptor(), kDescriptor);
    EXPECT_FALSE(lifetime_.RetirementPending());
}

TEST_F(ClientConnectionLifetimeTest, FailedTransactionBeforeCancellationAllowsImmediateRetirement)
{
    // Given a transaction whose transport fails after stopping is selected
    const auto result = lifetime_.ExecuteTransaction([this](auto) {
        EXPECT_TRUE(lifetime_.TrySetStopReason(StopReason::kUserRequested));
        return Result{score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE))};
    });
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().GetOsDependentErrorCode(), EPIPE);

    // When cancellation finishes after the transaction has returned
    const bool quiescent = lifetime_.CancellationFinished();

    // Then the caller may retire immediately without a redundant scheduled entry
    EXPECT_TRUE(quiescent);
    EXPECT_EQ(retirement_count_, 0U);
    EXPECT_EQ(lifetime_.RetireDescriptor(), kDescriptor);
}

TEST_F(ClientConnectionLifetimeTest, NestedTransactionReleaseDoesNotRetireOuterOperation)
{
    // Given an outer transaction which starts another admitted transaction
    const auto outer_result = lifetime_.ExecuteTransaction([this](auto) {
        const auto inner_result = lifetime_.ExecuteTransaction([this](auto) {
            // When cancellation completes with both operations active
            EXPECT_TRUE(lifetime_.TrySetStopReason(StopReason::kUserRequested));
            EXPECT_FALSE(lifetime_.CancellationFinished());
            return Result{};
        });

        // Then releasing the inner operation does not retire the outer operation
        EXPECT_TRUE(inner_result.has_value());
        EXPECT_EQ(retirement_count_, 0U);
        return Result{};
    });

    // Then only release of the final operation requests retirement
    EXPECT_TRUE(outer_result.has_value());
    EXPECT_EQ(retirement_count_, 1U);
    EXPECT_EQ(lifetime_.RetireDescriptor(), kDescriptor);
}

TEST_F(ClientConnectionLifetimeTest, QuiescentRetirementAllowsInstallingNextDescriptor)
{
    // Given a connection retired after all work has finished
    ASSERT_TRUE(lifetime_.TrySetStopReason(StopReason::kUserRequested));
    ASSERT_TRUE(lifetime_.CancellationFinished());
    ASSERT_EQ(lifetime_.RetireDescriptor(), kDescriptor);

    // When its owner starts the next lifecycle and installs a new descriptor
    reason_ = StopReason::kNone;
    lifetime_.InstallDescriptor(kDescriptor + 1);
    const auto result = lifetime_.ExecuteTransaction([](std::int32_t descriptor) {
        EXPECT_EQ(descriptor, kDescriptor + 1);
        return Result{};
    });

    // Then admission uses the new descriptor with no stale retirement state
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(lifetime_.IsAccepting());
    EXPECT_FALSE(lifetime_.RetirementPending());
    EXPECT_EQ(retirement_count_, 0U);
}

}  // namespace
}  // namespace score::message_passing::detail
