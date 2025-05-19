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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_NODE_IDENTIFIER_COPIER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_NODE_IDENTIFIER_COPIER_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"

#include <score/assert.hpp>

#include <array>
#include <shared_mutex>

namespace score::mw::com::impl::lola
{

using NodeIdTmpBufferType = std::array<pid_t, 20>;

/// \brief Copies node identifiers (pid) contained within (container) values of a map into a given buffer under
///        read-lock
/// \details
/// \tparam MapType Type of the Map. It is implicitly expected, that the key of MapType is of type ElementFqId and
///         the mapped_type is a std::set (or at least some forward iterable container type, which supports
///         lower_bound()), which contains values, which directly or indirectly contain a node identifier (pid_t)
///
/// \param event_id fully qualified event id for lookup in _src_map_
/// \param src_map map, where key_type = ElementFqId and mapped_type is some forward iterable container
/// \param src_map_mutex mutex to be used to lock the map during copying.
/// \param dest_buffer buffer, where to copy the node identifiers
/// \param start start identifier (pid_t) where to start search with.
///
/// \return pair containing number of node identifiers, which have been copied and a bool, whether further ids could
///         have been copied, if buffer would have been larger.
template <typename MapType>
// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be
// called implicitly". std::terminate() is implicitly called from 'dest_buffer.at()' in case the index goes outside
// the array's range, as we starts from index zero and we check that the buffer size is greater than zero and later
// we compare the index with the buffer's size, so no way for the index to go outside array's range and throw
// std::out_of_range which leds to std::terminate().
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
std::pair<std::uint8_t, bool> CopyNodeIdentifiers(ElementFqId event_id,
                                                  MapType& src_map,
                                                  std::shared_mutex& src_map_mutex,
                                                  NodeIdTmpBufferType& dest_buffer,
                                                  pid_t start) noexcept
{
    std::uint8_t num_nodeids_copied{0U};
    bool further_ids_avail{false};
    // Suppress "AUTOSAR C++14 M0-1-3" rule findings. This rule states: "There shall be no dead code".
    //  This is a RAII Pattern, which binds the life cycle of a resource that must be acquired before use.
    // coverity[autosar_cpp14_m0_1_3_violation]
    std::shared_lock<std::shared_mutex> read_lock(src_map_mutex);
    if (src_map.empty() == false)
    {
        auto search = src_map.find(event_id);
        if (search != src_map.end())
        {
            // we copy the target_node_identifiers to a tmp-array under lock ...
            for (auto iter = search->second.lower_bound(start); iter != search->second.cend(); iter++)
            {
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(dest_buffer.size() > 0U,
                                       "HandlerBase::CopyNodeIdentifiers destination buffer has size zero!");
                // Suppress "AUTOSAR C++14 M5-0-3" rule findings. This rule states: "A cvalue expression shall
                // not be implicitly converted to a different underlying type"
                // That return `pid_t` type as a result. Tolerated implicit conversion as underlying types are the
                // same coverity
                // coverity[autosar_cpp14_m5_0_3_violation]
                dest_buffer.at(num_nodeids_copied) = *iter;
                num_nodeids_copied++;
                if (num_nodeids_copied == dest_buffer.size())
                {
                    if (std::distance(iter, search->second.cend()) > 1)
                    {
                        further_ids_avail = true;
                    }
                    break;
                }
            }
        }
    }
    return {num_nodeids_copied, further_ids_avail};
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_NODE_IDENTIFIER_COPIER_H
