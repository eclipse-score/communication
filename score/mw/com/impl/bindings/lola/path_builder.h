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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_PATH_BUILDER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_PATH_BUILDER_H

#include "score/mw/com/impl/configuration/lola_service_instance_id.h"

#include <optional>
#include <sstream>
#include <string>

namespace score::mw::com::impl::lola
{

/// Create a string with the given prefix and a callable that emits the remaining shm path. Can fail if emitter returns
/// false.
///
/// \tparam Emitter Type of callable that, when called with an ostream reference, emits the shm path after the
//          prefix, without a leading slash.
/// \tparam Prefix Type of the prefix that is to be put in front of the full path
/// \param prefix Prefix that is to be put in front of the full path
/// \param emitter Callable that, when called with an ostream reference, emits the shm path after the prefix,
///                without a leading slash.
/// \return String that contains the path, or nullopt in case of an error.
template <typename Emitter, typename Prefix>
std::optional<std::string> OptionalEmitWithPrefix(Prefix prefix, Emitter emitter) noexcept
{
    std::stringstream out{};
    out << prefix;
    if (emitter(out))
    {
        return out.str();
    }
    else
    {
        return std::nullopt;
    }
}

/// Create a string with the given prefix and a callable that emits the remaining shm path
///
/// \tparam Emitter Type of callable that, when called with an ostream reference, emits the shm path after the
//          prefix, without a leading slash.
/// \tparam Prefix Type of the prefix that is to be put in front of the full path
/// \param prefix Prefix that is to be put in front of the full path
/// \param emitter Callable that, when called with an ostream reference, emits the shm path after the prefix,
///                without a leading slash.
/// \return String that contains the path
template <typename Emitter, typename Prefix>
std::string EmitWithPrefix(Prefix prefix, Emitter emitter) noexcept
{
    std::stringstream out{};
    out << prefix;
    emitter(out);
    return out.str();
}

/// Append a string of the form XXXXXXXXXXXXXXXX-YYYY where X is the
/// service id and Y is the instance id.
///
/// \pre instanceId of binding must be available.
///
/// \param out Stream to write the string to.
void AppendServiceAndInstance(std::ostream& out,
                              const std::uint16_t service_id,
                              const LolaServiceInstanceId::InstanceId instance_id) noexcept;

void AppendService(std::ostream& out, const std::uint16_t service_id) noexcept;
void AppendInstanceId(std::ostream& out, const LolaServiceInstanceId::InstanceId instance_id) noexcept;

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_PATH_BUILDER_H
