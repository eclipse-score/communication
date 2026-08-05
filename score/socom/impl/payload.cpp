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

#include "score/socom/payload.hpp"

#include <cassert>

namespace score::cpp {

template <typename T>
bool operator==(span<T> const& lhs, span<T> const& rhs) {
    return std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs), std::end(rhs));
}
}  // namespace score::cpp

namespace score::socom {

namespace detail {
bool Payload_impl::operator==(Payload_impl const& other) const noexcept {
    return header() == other.header() && data() == other.data();
}
}  // namespace detail

Payload empty_payload() {
    return Payload{Payload::Writable_span{}, kNoSlotHandle, []() {}};
}

}  // namespace score::socom
