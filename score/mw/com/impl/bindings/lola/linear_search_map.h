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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_LINEAR_SEARCH_MAP_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_LINEAR_SEARCH_MAP_H

#include "score/containers/non_relocatable_vector.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <score/assert.hpp>

#include <cstddef>
#include <functional>
#include <scoped_allocator>
#include <tuple>
#include <utility>

namespace score::mw::com::impl::lola
{

/// \brief A fixed-capacity, associative container with a std::map-like interface backed by a contiguous array.
///
/// \details LinearSearchMap replaces the previously used std::map / boost::interprocess::map inside the shared-memory
/// data structures (e.g. ServiceDataStorage). Those map-types allocate memory dynamically on each insertion
/// (the exact amount depends on the implementation) which makes it impossible to calculate the exact size of the
/// surrounding shared-memory object up-front (without a costly simulation run). LinearSearchMap in contrast allocates
/// all of its storage exactly once on construction (via score::containers::NonRelocatableVector) with a capacity equal
/// to the maximum number of elements. This capacity is always known in advance (it equals the number of
/// service-elements of a service-instance). Hence the memory needed by a LinearSearchMap can be calculated
/// deterministically.
///
/// Since the number of elements is small (number of events + fields of a single service-instance) and the container is
/// filled once during initialization and only searched afterwards, a simple linear search is sufficient.
///
/// LinearSearchMap is designed to be placeable in shared-memory: it uses the provided (shared-memory capable) allocator
/// and NonRelocatableVector guarantees that no reallocation (which would invalidate offset-pointers) ever takes place.
///
/// \note The complexity for lookup (find/at) is currently O(n). Since we typically use LinearSearchMap for a small
/// number of elements (number of events + fields of a single service-instance), this is acceptable. If the number of
/// elements grows, we could sort the underlying vector and then use std::binary_search for O(log n) lookup.
/// This should be done if necessary.
///
/// \attention Adding more elements to it as defined in the ctor via max_number_of_elements will lead to termination.
///
/// \tparam Key key type used to identify an element. Must be copy-constructible and comparable via KeyEqual.
/// \tparam MappedType mapped value type.
/// \tparam KeyEqual binary predicate used to compare two keys for equality. Defaults to std::equal_to<Key> (i.e.
///         operator==). A custom predicate can be supplied for keys without a suitable operator== or for alternative
///         equality semantics (analogous to the KeyEqual parameter of std::unordered_map). Be careful: When placing
///         the LinearSearchMap in shared-memory, the KeyEqual predicate must be stateless and trivially copyable
///         (i.e. it must not contain any non-offset pointers or references).
/// \tparam Allocator allocator used for the underlying storage. Defaults to the shared-memory capable
///         PolymorphicOffsetPtrAllocator.
template <typename Key,
          typename MappedType,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator =
              std::scoped_allocator_adaptor<memory::shared::PolymorphicOffsetPtrAllocator<std::pair<Key, MappedType>>>>
class LinearSearchMap
{
  public:
    using key_type = Key;
    using mapped_type = MappedType;
    using value_type = std::pair<Key, MappedType>;
    using size_type = std::size_t;
    using key_equal = KeyEqual;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    /// \brief Constructs an empty LinearSearchMap which can hold up to max_number_of_elements elements.
    /// \param max_number_of_elements maximum number of elements the map can hold (its fixed capacity).
    /// \param resource memory resource used to allocate the underlying storage.
    /// \param key_equal binary predicate used to compare keys for equality.
    LinearSearchMap(const size_type max_number_of_elements,
                    memory::shared::ManagedMemoryResource& resource,
                    const KeyEqual& key_equal = KeyEqual{});

    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    const_iterator cbegin() const noexcept;

    iterator end() noexcept;
    const_iterator end() const noexcept;
    const_iterator cend() const noexcept;

    size_type size() const noexcept;

    size_type capacity() const noexcept;

    bool empty() const noexcept;

    iterator find(const Key& key) noexcept;

    const_iterator find(const Key& key) const noexcept;

    /// \brief Returns the key-equality predicate used by this map.
    key_equal key_eq() const;

    mapped_type& at(const Key& key);

    const mapped_type& at(const Key& key) const;

    /// \brief Inserts a new element constructed in-place from the given piecewise tuples (std::map-like).
    /// \details If an element with the same key already exists, no insertion takes place.
    /// \attention emplacing more elements as specified in ctor max_number_of_elements will lead to termination.
    /// \return pair of an iterator to the (existing or newly inserted) element and a bool that is true if an insertion
    ///         took place.
    template <typename... KeyArgs, typename... MappedArgs>
    std::pair<iterator, bool> emplace(std::piecewise_construct_t,
                                      std::tuple<KeyArgs...> key_args,
                                      std::tuple<MappedArgs...> mapped_args);

    /// \brief Inserts a new element with the given key and mapped value (std::map-like).
    /// \details If an element with the same key already exists, no insertion takes place.
    /// \attention emplacing more elements as specified in ctor max_number_of_elements will lead to termination.
    template <typename MappedArg>
    std::pair<iterator, bool> emplace(const Key& key, MappedArg&& mapped_value);

  private:
    score::containers::NonRelocatableVector<value_type, Allocator> storage_;
    KeyEqual key_equal_;
};

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::LinearSearchMap(const size_type max_number_of_elements,
                                                                       memory::shared::ManagedMemoryResource& resource,
                                                                       const KeyEqual& key_equal)
    : storage_{max_number_of_elements, Allocator{resource}}, key_equal_{key_equal}
{
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::begin() noexcept
{
    return storage_.begin();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::const_iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::begin() const noexcept
{
    return storage_.begin();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::const_iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::cbegin() const noexcept
{
    return storage_.cbegin();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::end() noexcept
{
    return storage_.end();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::const_iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::end() const noexcept
{
    return storage_.end();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::const_iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::cend() const noexcept
{
    return storage_.cend();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::size_type
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::size() const noexcept
{
    return storage_.size();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::size_type
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::capacity() const noexcept
{
    return storage_.capacity();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
bool LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::empty() const noexcept
{
    return storage_.size() == 0U;
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::find(const Key& key) noexcept
{
    for (auto it = begin(); it != end(); ++it)
    {
        if (key_equal_(it->first, key))
        {
            return it;
        }
    }
    return end();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::const_iterator
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::find(const Key& key) const noexcept
{
    for (auto it = cbegin(); it != cend(); ++it)
    {
        if (key_equal_(it->first, key))
        {
            return it;
        }
    }
    return cend();
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::key_equal
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::key_eq() const
{
    return key_equal_;
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::mapped_type&
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::at(const Key& key)
{
    const auto it = find(key);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(it != end(), "LinearSearchMap::at: key not found.");
    return it->second;
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
const typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::mapped_type&
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::at(const Key& key) const
{
    const auto it = find(key);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(it != cend(), "LinearSearchMap::at: key not found.");
    return it->second;
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
template <typename... KeyArgs, typename... MappedArgs>
std::pair<typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::iterator, bool>
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::emplace(std::piecewise_construct_t,
                                                               std::tuple<KeyArgs...> key_args,
                                                               std::tuple<MappedArgs...> mapped_args)
{
    const Key key = std::make_from_tuple<Key>(key_args);
    const auto existing = find(key);
    if (existing != end())
    {
        return {existing, false};
    }
    auto& inserted = storage_.emplace_back(std::piecewise_construct, std::move(key_args), std::move(mapped_args));
    return {&inserted, true};
}

template <typename Key, typename MappedType, typename KeyEqual, typename Allocator>
template <typename MappedArg>
std::pair<typename LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::iterator, bool>
LinearSearchMap<Key, MappedType, KeyEqual, Allocator>::emplace(const Key& key, MappedArg&& mapped_value)
{
    const auto existing = find(key);
    if (existing != end())
    {
        return {existing, false};
    }
    auto& inserted = storage_.emplace_back(std::piecewise_construct,
                                           std::forward_as_tuple(key),
                                           std::forward_as_tuple(std::forward<MappedArg>(mapped_value)));
    return {&inserted, true};
}

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_LINEAR_SEARCH_MAP_H
