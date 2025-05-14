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
#ifndef SCORE_LIB_MEMORY_SHARED_MAP_H
#define SCORE_LIB_MEMORY_SHARED_MAP_H

#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#if defined(__linux__)
#include <boost/container/scoped_allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#endif  // __linux__

#include <map>
#include <scoped_allocator>

namespace score::memory::shared
{

/// \brief We provide our custom version of an std::map to ensure that it supports HEAP and SharedMemory usage with
/// our custom allocator. In addition we ensure nested container usage by using the scoped allocator adaptor.

// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Rationale: Pre-processor commands are used to allow different implementations for linux and QNX to exist in the same
// file. This keeps both implementations close (i.e. within the same functions) which makes the code easier to read and
// maintain. It also prevents compiler errors in linux code when compiling for QNX and vice versa.
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined(__linux__)
template <typename K, typename V, typename Comp = std::less<K>>
using Map =
    boost::interprocess::map<K,
                             V,
                             Comp,
                             boost::container::scoped_allocator_adaptor<
                                 PolymorphicOffsetPtrAllocator<typename boost::interprocess::map<K, V>::value_type>>>;
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#else
// on production with libc++ stl, we should use this!
template <typename K, typename V, typename Comp = std::less<K>>
using Map = std::
    map<K, V, Comp, std::scoped_allocator_adaptor<PolymorphicOffsetPtrAllocator<typename std::map<K, V>::value_type>>>;
// coverity[autosar_cpp14_a16_0_1_violation] Different implementation required for linux and QNX
#endif  // __linux__

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_MAP_H
