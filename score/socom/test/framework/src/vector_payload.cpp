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

#include "score/socom/vector_payload.hpp"

#include <cassert>
#include <memory>

namespace score::socom {

Payload make_vector_payload(Vector_buffer buffer) {
    auto buf = std::make_unique<Vector_buffer>(std::move(buffer));
    auto span = Payload::Writable_span{*buf};
    return Payload{span, kNoSlotHandle, [buf = std::move(buf)]() {}};
}

Payload make_vector_payload(std::size_t header_size, Vector_buffer buffer) {
    assert(header_size <= buffer.size());
    auto buf = std::make_unique<Vector_buffer>(std::move(buffer));
    auto span = Payload::Writable_span{*buf};
    return Payload{span, kNoSlotHandle, [buf = std::move(buf)]() {}, header_size};
}

Payload make_vector_payload(std::size_t lead_offset, std::size_t header_size,
                            Vector_buffer buffer) {
    assert((lead_offset + header_size) <= buffer.size());
    auto buf = std::make_unique<Vector_buffer>(std::move(buffer));
    auto span = Payload::Writable_span{*buf};
    return Payload{span, kNoSlotHandle, [buf = std::move(buf)]() {}, header_size, lead_offset};
}

Writable_payload make_writable_vector_payload(std::size_t size) {
    auto buf = std::make_unique<std::vector<std::byte>>(size);
    auto span = Writable_payload::Writable_span{*buf};
    return Writable_payload{span, kNoSlotHandle, [buf = std::move(buf)]() {}};
}

Writable_payload make_writable_vector_payload(std::size_t lead_offset, std::size_t header_size,
                                              Vector_buffer buffer) {
    assert((lead_offset + header_size) <= buffer.size());
    auto buf = std::make_unique<Vector_buffer>(std::move(buffer));
    auto span = Writable_payload::Writable_span{*buf};
    return Writable_payload{span, kNoSlotHandle, [buf = std::move(buf)]() {}, header_size,
                            lead_offset};
}

Payload clone_payload(Payload const& p) {
    auto header = p.header();
    auto data = p.data();
    Vector_buffer buf(header.size() + data.size());
    std::copy(header.begin(), header.end(), buf.begin());
    std::copy(data.begin(), data.end(), buf.begin() + static_cast<std::ptrdiff_t>(header.size()));
    return make_vector_payload(header.size(), std::move(buf));
}

}  // namespace score::socom
