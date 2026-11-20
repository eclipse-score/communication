/********************************************************************************
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
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_OSABSTRACTION_MULTIPLEXER_MULTIPLEXER
#define LG_SOMEIP_OSABSTRACTION_MULTIPLEXER_MULTIPLEXER

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace lgsomeip
{
namespace osabstraction
{

/**
 * @brief One thread that waits for readable file descriptors and runs the callback set for each.
 * The wait runs on io_dispatcher: poll() on Linux, ionotify() pulses on QNX.
 * A pass that ends after an idle second also calls the timeout callback.
 * On Linux, every pass also runs TimerManager::iterate_loop().
 * @pre One owner calls set() and unset() for an fd, never two threads at once.
 * After its unset() returns, the owner closes the fd, and its number can belong to another socket at once.
 * @note A failed wait is logged and ends the thread. The fds stay set, and no callback runs any more.
 */
class Multiplexer
{
  public:
    /**
     * @brief Creates the reactor. The thread starts in start().
     * @throws std::runtime_error when the reactor cannot get its OS resources.
     */
    Multiplexer();

    /**
     * @brief Stops and joins the thread, then destroys every callback that is still set.
     * @pre The destructor does not run inside a callback of this multiplexer, because the thread would join itself.
     */
    virtual ~Multiplexer();

    void start();

    /**
     * @brief Wakes the thread and makes it leave its loop. join() waits for that.
     */
    void stop();

    void join();

    /**
     * @brief Calls `callback` on the multiplexer thread each time `file_descriptor` is readable.
     * @param[in] callback Receives `false` for data, hang-up, and error alike.
     * The callback detects end of stream from a read that returns 0.
     * @note A negative fd, an empty callback, an fd that is already set, and a full reactor are logged and ignored.
     * @warning `callback` must not own anything whose destructor calls set() or unset().
     * The multiplexer can destroy a callback while it holds its mutex, so that call deadlocks.
     */
    void set(int file_descriptor, std::function<void(bool)> callback);

    /**
     * @brief Calls `callback` on the multiplexer thread when a pass ends after an idle second.
     */
    void set_timeout(std::function<void()> callback);

    /**
     * @brief Stops the dispatch to `file_descriptor`. An fd that is not set is ignored.
     * @note No callback of the fd starts after this returns.
     * Called from another thread, it waits until a running callback of the fd returns and destroys it.
     * Called from the fd's own callback, it returns at once, and the callback is destroyed after it returns.
     * @warning Do not call it while holding a lock that the fd's callback takes, because both threads then wait.
     */
    void unset(int file_descriptor);

    void run();

  private:
    struct Backend;

    std::unique_ptr<Backend> backend_;
    std::atomic_bool running_{true};
    std::shared_ptr<std::thread> thread_;
};

}  // namespace osabstraction
}  // namespace lgsomeip

#endif  // LG_SOMEIP_OSABSTRACTION_MULTIPLEXER_MULTIPLEXER
