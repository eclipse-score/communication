#ifndef SCORE_LIB_MESSAGE_PASSING_CONNECTION_LIFETIME_H
#define SCORE_LIB_MESSAGE_PASSING_CONNECTION_LIFETIME_H

#include "score/message_passing/i_client_connection.h"

#include <score/assert.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <utility>

namespace score::message_passing::detail
{

class ClientSendQueue;

class ClientConnectionLifetime final
{
  public:
    using State = IClientConnection::State;
    using StopReason = IClientConnection::StopReason;
    using RetirementCallback = score::cpp::callback<void()>;

    class Operation final
    {
      public:
        Operation(ClientConnectionLifetime& lifetime, std::unique_lock<std::mutex>& lock) noexcept
            : lifetime_{lifetime}, lock_{lock}, admitted_{lifetime.IsAccepting()}
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(lock_.owns_lock());
            SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(lock_.mutex() == &lifetime_.mutex_);
            if (admitted_)
            {
                ++lifetime_.active_operations_;
            }
        }

        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;
        Operation(Operation&&) = delete;
        Operation& operator=(Operation&&) = delete;

        ~Operation() noexcept
        {
            if (admitted_)
            {
                if (!lock_.owns_lock())
                {
                    lock_.lock();
                }
                --lifetime_.active_operations_;
                if ((lifetime_.active_operations_ == 0) && lifetime_.retirement_pending_)
                {
                    lifetime_.schedule_retirement_();
                }
            }
        }

        explicit operator bool() const noexcept
        {
            return admitted_;
        }

        template <typename Actor>
        auto Execute(Actor&& actor) noexcept
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(admitted_);
            lock_.unlock();
            auto result = std::forward<Actor>(actor)(lifetime_.descriptor_);
            lock_.lock();
            return result;
        }

      private:
        ClientConnectionLifetime& lifetime_;
        std::unique_lock<std::mutex>& lock_;
        const bool admitted_;
    };

    ClientConnectionLifetime(const std::atomic<State>& state,
                             std::atomic<StopReason>& stop_reason,
                             RetirementCallback schedule_retirement) noexcept
        : state_{state},
          stop_reason_{stop_reason},
          schedule_retirement_{std::move(schedule_retirement)},
          mutex_{},
          descriptor_{-1},
          active_operations_{0},
          retirement_pending_{false}
    {
    }

    bool IsAccepting() const noexcept
    {
        return (state_ == State::kReady) && (stop_reason_ == StopReason::kNone);
    }

    bool TrySetStopReason(StopReason reason) noexcept
    {
        auto lock = Lock();
        StopReason expected = StopReason::kNone;
        return stop_reason_.compare_exchange_strong(expected, reason);
    }

    void InstallDescriptor(std::int32_t descriptor) noexcept
    {
        auto lock = Lock();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(active_operations_ == 0);
        descriptor_ = descriptor;
    }

    bool RetirementPending() noexcept
    {
        auto lock = Lock();
        return retirement_pending_;
    }

    bool CancellationFinished() noexcept
    {
        auto lock = Lock();
        retirement_pending_ = true;
        return active_operations_ == 0;
    }

    std::int32_t RetireDescriptor() noexcept
    {
        auto lock = Lock();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(retirement_pending_);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(active_operations_ == 0);
        retirement_pending_ = false;
        return std::exchange(descriptor_, -1);
    }

    template <typename Actor>
    auto ExecuteTransaction(Actor&& actor) noexcept -> decltype(std::forward<Actor>(actor)(std::int32_t{}))
    {
        auto lock = Lock();
        Operation operation{*this, lock};
        if (!operation)
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL));
        }
        return operation.Execute(std::forward<Actor>(actor));
    }

  private:
    friend class ClientSendQueue;

    std::unique_lock<std::mutex> Lock() noexcept
    {
        return std::unique_lock<std::mutex>{mutex_};
    }

    std::mutex& Mutex() noexcept
    {
        return mutex_;
    }

    const std::atomic<State>& state_;
    std::atomic<StopReason>& stop_reason_;
    RetirementCallback schedule_retirement_;
    std::mutex mutex_;
    std::int32_t descriptor_;
    std::uint32_t active_operations_;
    bool retirement_pending_;
};

}  // namespace score::message_passing::detail

#endif
