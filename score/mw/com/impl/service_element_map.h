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
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///

#ifndef SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_H
#define SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_H

#include <map>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Map that will be used in GenericProxies to store GenericProxyEvents and possibly GenericProxyFields and
/// GenericProxyMethods once they're supported by LoLa.
template <typename Value>
class ServiceElementMap
{
  public:
    using Key = std::string_view;
    using map_type = std::map<Key, Value>;
    using value_type = std::pair<const Key, Value>;
    using iterator = typename std::map<Key, Value>::iterator;
    using const_iterator = typename std::map<Key, Value>::const_iterator;
    using size_type = typename std::map<Key, Value>::size_type;

    std::pair<iterator, bool> insert(const value_type& value)
    {
        return map_.insert(value);
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        return map_.emplace(std::forward<Args>(args)...);
    }

    iterator erase(const_iterator pos)
    {
        return map_.erase(pos);
    }

    size_type erase(const Key& key)
    {
        return map_.erase(key);
    }

    const_iterator cbegin() const noexcept
    {
        return map_.cbegin();
    }

    const_iterator cend() const noexcept
    {
        return map_.cend();
    }

    iterator find(const Key& search_key) noexcept
    {
        return map_.find(search_key);
    }

    const_iterator find(const Key& search_key) const noexcept
    {
        return map_.find(search_key);
    }

    size_type size() const noexcept
    {
        return map_.size();
    }

    bool empty() const noexcept
    {
        return map_.empty();
    }

  private:
    map_type map_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_H
