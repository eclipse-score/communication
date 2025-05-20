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

#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SYNC_UTILS_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SYNC_UTILS_H

#include "score/concurrency/future/interruptible_promise.h"

#include <amp_jthread.hpp>
#include <amp_stop_token.hpp>

namespace bmw
{
  namespace mw
  {
    namespace com
    {
      namespace test
      {

        class SyncCoordinator
        {
        public:
          SyncCoordinator(amp::string_view file_name);
          ~SyncCoordinator() = default;
          void Signal() noexcept;
          static void CleanUp(amp::string_view file_name) noexcept;
          bmw::concurrency::InterruptibleFuture<void> Wait(const amp::stop_token &stop_token) noexcept;

        private:
          void CheckFileCreation(std::shared_ptr<bmw::concurrency::InterruptiblePromise<void>> promise,
                                 const amp::stop_token &stop_token) noexcept;
          amp::string_view file_name_;
          amp::jthread checkfile_thread_;
        };

      } // namespace test
    } // namespace com
  } // namespace mw
} // namespace bmw

#endif // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_SYNC_UTILS_H
