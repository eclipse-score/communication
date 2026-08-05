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

#ifndef SCORE_SOCOM_VECTOR_PAYLOAD_HPP
#define SCORE_SOCOM_VECTOR_PAYLOAD_HPP

#include <cstddef>
#include <vector>

#include "score/socom/payload.hpp"

namespace score::socom {

/// \brief Alias for payload data.
using Vector_buffer = std::vector<Payload::Byte>;

/// \brief Creates a Vector_buffer from a list of unsigned integral type elements.
/// \param args List of unsigned integral type elements to be included in the Vector_buffer.
/// \return A Vector_buffer containing the provided elements.
template <typename... Ts>
Vector_buffer make_vector_buffer(Ts... args) noexcept {
    static_assert((std::is_unsigned_v<Ts> && ...),
                  "All arguments must be unsigned integral types.");
    // TODO check that the types are not larger than Payload::Byte
    return {static_cast<Payload::Byte>(args)...};
}

/// \brief Creates a vector payload by moving the given data.
/// \param buffer Payload data.
/// \return A Payload object.
Payload make_vector_payload(Vector_buffer buffer);

/// \brief Creates a vector payload by moving the given data.
/// \param header_size Size of header data.
/// \param buffer Payload data.
/// \return A Payload object.
Payload make_vector_payload(std::size_t header_size, Vector_buffer buffer);

Payload make_vector_payload(std::size_t lead_offset, std::size_t header_size, Vector_buffer buffer);

/// \brief Creates vector payload from a container.
/// \param container Reference container.
/// \tparam C Container type.
/// \return A Payload object.
template <typename C>
inline Payload make_vector_payload(C const& container) {
    return make_vector_payload(Vector_buffer{std::begin(container), std::end(container)});
}

/// \brief Creates a test Writable_payload backed by a heap-allocated buffer.
/// \param size Size of the payload buffer in bytes.
/// \return A Writable_payload object.
Writable_payload make_writable_vector_payload(std::size_t size);

/// \brief Creates a test Writable_payload backed by a heap-allocated buffer.
/// \param lead_offset Size of the lead offset in bytes.
/// \param header_size Size of the header in bytes.
/// \param buffer Payload data.
/// \return A Writable_payload object.
Writable_payload make_writable_vector_payload(std::size_t lead_offset, std::size_t header_size,
                                              Vector_buffer buffer);

/// \brief Creates a copy of a payload's data as a new vector payload (test utility).
Payload clone_payload(Payload const& p);

}  // namespace score::socom

#endif  // SCORE_SOCOM_VECTOR_PAYLOAD_HPP
