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

#ifndef SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_VIEW_H
#define SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_VIEW_H

#include <functional>
#include <map>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

template <typename ElementType>
class ServiceElementMapViewFactory;

/// \brief A map view that will be provided to the user in case of GenericProxy and GenericSkeleton to allow access to
///        the GenericProxyEvents/GenericSkeletonEvents and possibly fields/methods in a later stage. The view is NOT
///        owning. I.e. the view is set up by the owner of the underlying map and provides a read-only view to this map.
template <typename Value>
class ServiceElementMapView
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // The "ServiceElementMapViewFactory" class is a helper, which allows to hide the ctor of this class from the user,
    // which shall not be able to construct instances on their own.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ServiceElementMapViewFactory<Value>;

  public:
    using key = std::string_view;
    using value_type = std::pair<const key, Value>;
    using iterator = typename std::map<key, Value>::iterator;
    using const_iterator = typename std::map<key, Value>::const_iterator;
    using size_type = typename std::map<key, Value>::size_type;

    const_iterator cbegin() const noexcept
    {
        return elements_.get().cbegin();
    }

    const_iterator cend() const noexcept
    {
        return elements_.get().cend();
    }

    iterator find(const key& search_key) noexcept
    {
        return elements_.get().find(search_key);
    }

    const_iterator find(const key& search_key) const noexcept
    {
        return elements_.get().find(search_key);
    }

    size_type size() const noexcept
    {
        return elements_.get().size();
    }

    bool empty() const noexcept
    {
        return elements_.get().empty();
    }

  private:
    using map_type = std::map<key, Value>;

    /// \brief Creates a ServiceElementMapView that provides "read-only" access to the provided map. That is no elements
    ///        can be added or removed from the map via the ServiceElementMapView, but changes to the map elements is
    ///        possible.
    /// \details ctor is private. Creation shall be done via ServiceElementMapView<>::Create().
    /// @param service_element_map underlying map on which the view is created. It must not be moved or copied after
    ///        being passed to this ctor.
    ServiceElementMapView(map_type& service_element_map) noexcept : elements_{service_element_map} {}

    std::reference_wrapper<map_type> elements_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_VIEW_H
