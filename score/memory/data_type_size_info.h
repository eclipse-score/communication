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
#ifndef SCORE_LIB_MEMORY_DATA_TYPE_SIZE_INFO_H
#define SCORE_LIB_MEMORY_DATA_TYPE_SIZE_INFO_H

#include <score/assert.hpp>

#include <cstddef>

namespace score::memory
{

class DataTypeSizeInfo
{
  public:
    constexpr DataTypeSizeInfo(const std::size_t size, const std::size_t alignment) : size_{size}, alignment_{alignment}
    {
        const bool is_alignment_power_of_two = ((alignment != 0) && ((alignment & (alignment - 1)) == 0));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(is_alignment_power_of_two, "The standard requires that alignment is a power of 2!");

        const bool is_size_multiple_of_alignment = (size % alignment == 0);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(is_size_multiple_of_alignment,
                               "The standard requires that size is a multiple of alignment!");
    }

    constexpr std::size_t Size() const
    {
        return size_;
    }

    constexpr std::size_t Alignment() const
    {
        return alignment_;
    }

  private:
    std::size_t size_;
    std::size_t alignment_;
};

bool operator==(const DataTypeSizeInfo& lhs, const DataTypeSizeInfo& rhs) noexcept;

}  // namespace score::memory

#endif  // SCORE_LIB_MEMORY_DATA_TYPE_SIZE_INFO_H
