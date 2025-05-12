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
