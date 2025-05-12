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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_ROLLBACK_SYNCHRONIZATION_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_ROLLBACK_SYNCHRONIZATION_H

#include <mutex>
#include <ostream>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl::lola
{

class ServiceDataControl;

/// \brief Manages the synchronization, when multiple Proxy instances (ProxyElements) for the same service instance
///        within one LoLa process are going to initiate their Proxy transaction log rollback.
/// \details This is for the seldom/pathological case, where we have several proxy instances within a LoLa process,
///          which relate to the same (provided) service instance. This case needs some special care/synchronization as
///          these proxy instances work on some shared transaction log resources, where special synchronization needs
///          to take place.
class RollbackSynchronization
{
  public:
    /// \brief Returns a mutex for the given proxy_element_control. Either an existing one or creates one, if not yet
    ///        existent.
    /// \return a pair, consisting of a ref to the mutex for the given proxy_element_control and a bool, indicating,
    ///         whether the mutex already existed (true) or had been created within the call (false)
    std::pair<std::mutex&, bool> GetMutex(const ServiceDataControl* proxy_element_control);

  private:
    std::unordered_map<const ServiceDataControl*, std::mutex> synchronisation_data_map_{};
    /// \brief mutex to synchronize access to synchronisation_data_map_
    std::mutex map_mutex_{};
};

// Due to a bug / implementation decision in gtest, the default print function used by gtest to print an object (e.g.
// when it's returned by EXPECT_CALL / ON_CALL) is not thread safe. Therefore, we implement an ostream overload so that
// gtest will use this function which is thread safe. This is to prevent failures when running threaded tests under tsan
// (Ticket-183866).
std::ostream& operator<<(std::ostream& ostream_out, const RollbackSynchronization&) noexcept;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_ROLLBACK_SYNCHRONIZATION_H
