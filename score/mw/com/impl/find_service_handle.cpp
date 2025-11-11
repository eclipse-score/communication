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
#include "score/mw/com/impl/find_service_handle.h"

#include "score/mw/log/logging.h"

namespace score::mw::com::impl
{

FindServiceHandle::FindServiceHandle(const std::size_t uid) : uid_{uid} {}

auto operator==(const FindServiceHandle& lhs, const FindServiceHandle& rhs) noexcept -> bool
{
    return lhs.uid_ == rhs.uid_;
}

auto operator<(const FindServiceHandle& lhs, const FindServiceHandle& rhs) noexcept -> bool
{
    return lhs.uid_ < rhs.uid_;
}

// Suppress "AUTOSAR C++14 A13-2-2", The rule states: "A binary arithmetic operator and a bitwise operator shall return
// a “prvalue”." The code with '<<' is not a left shift operator but an overload for logging the respective types.
// coverity[autosar_cpp14_a13_2_2_violation : FALSE]
mw::log::LogStream& operator<<(mw::log::LogStream& log_stream, const FindServiceHandle& find_service_handle)
{
    log_stream << find_service_handle.uid_;
    return log_stream;
}

std::ostream& operator<<(std::ostream& ostream_out, const FindServiceHandle& find_service_handle)
{
    ostream_out << find_service_handle.uid_;
    return ostream_out;
}

auto make_FindServiceHandle(const std::size_t uid) -> FindServiceHandle
{
    return FindServiceHandle(uid);
}

}  // namespace score::mw::com::impl

namespace std
{

std::size_t hash<score::mw::com::impl::FindServiceHandle>::operator()(
    const score::mw::com::impl::FindServiceHandle& find_service_handle) const noexcept
{
    score::mw::com::impl::FindServiceHandleView view{find_service_handle};
    return view.getUid();
}

}  // namespace std
