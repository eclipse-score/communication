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
 ********************************************************************************/
#ifndef SCORE_IO_DISPATCHER_IO_DISPATCHER_FACTORY_H
#define SCORE_IO_DISPATCHER_IO_DISPATCHER_FACTORY_H

/**
 * @file io_dispatcher_factory.h
 * @brief Platform-agnostic construction entry point for the reactor.
 *
 * Callers create a reactor through MakeIoDispatcher() and hold it as an IoDispatcher.
 * The concrete backend (Linux poll+self-pipe, QNX channel+pulses) is selected at link time, so no consumer needs
 * the platform headers or a `#if defined(__QNX__)` switch.
 */

#include "score/io_dispatcher/io_dispatcher.h"
#include "score/result/result.h"
#include <cstdint>
#include <memory>

namespace io_dispatcher
{

/**
 * @brief Creates the reactor backend for the target platform.
 * @param[in] max_num_event_sources Maximum number of concurrently registered event sources.
 * @param[in] tag Stamped into every handle the reactor issues; see DispatcherTagOf().
 * @return An owned reactor, `kApiError` when `tag` exceeds kMaxDispatcherTag, or `kResource` when
 *         `max_num_event_sources` exceeds the number the backend allows or a platform resource cannot be acquired.
 */
score::Result<std::unique_ptr<IoDispatcher>> MakeIoDispatcher(const std::uint16_t max_num_event_sources,
                                                              const DispatcherTag tag = 0U) noexcept;

/**
 * @brief Largest `max_num_event_sources` MakeIoDispatcher() accepts on this platform.
 */
std::uint16_t MaxNumEventSources() noexcept;

}  // namespace io_dispatcher

#endif  // SCORE_IO_DISPATCHER_IO_DISPATCHER_FACTORY_H
