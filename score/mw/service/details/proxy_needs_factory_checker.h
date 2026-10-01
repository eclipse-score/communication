/*********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#ifndef SCORE_MW_SERVICE_DETAILS_PROXY_NEEDS_FACTORY_CHECKER_H
#define SCORE_MW_SERVICE_DETAILS_PROXY_NEEDS_FACTORY_CHECKER_H

#include "score/mw/service/details/proxy_builder.h"
#include "score/mw/service/details/type_holder.h"

#include <score/utility.hpp>
#include <tuple>
#include <utility>

namespace score::mw::service::details
{

// Base case for class specialization explained below
template <typename>
class ProxyNeedsFactoryChecker
{
};

// We use class specialization for being able to extract the inners of a tuple
template <typename... ProxySpec>
class ProxyNeedsFactoryChecker<std::tuple<ProxySpec...>>
{
  public:
    template <typename... Strategies>
    static void AreCompatible()
    {
        static_assert(sizeof...(ProxySpec) == sizeof...(Strategies),
                      "Number of required proxies does not match number of given strategies.");

        // We put the parameter packs into tuples and zip them into a pair, using argument expansion of parameter packs
        // We need the type holder, since we need to pass constructed types down the iteration. But the real types might
        // not be default constructible (or copyable)
        IterateOverTupleAndParameterPack(
            std::pair<TypeHolder<ProxySpec>,
                      TypeHolder<typename details::ProxySpecTraits<typename Strategies::BaseProxy>::HolderType>>{}...);
    }

  private:
    // In order to iterate over tuples, we have to recursively call these functions and execute the required checks on
    // each tuple element.
    template <typename Head, typename... Rest>
    static void IterateOverTupleAndParameterPack(const Head& head, Rest... rest_of_tuple)
    {
        score::cpp::ignore = head;
        using TypeRequestedByProxyNeeds = typename Head::first_type::Types;
        using TypeProvidedByStrategy = typename Head::second_type::Types;
        EnsureCanConvertFromTo<TypeProvidedByStrategy, TypeRequestedByProxyNeeds>();

        IterateOverTupleAndParameterPack(rest_of_tuple...);
    }

    // This is the base case for the recursive tuple iteration
    static void IterateOverTupleAndParameterPack() {}

    // In order to give nice error messages, we put the check into a custom function. Now a user, in case of assertion
    // error, must only search for the template parameter name in the compile error, and he can see which interface is
    // affected.
    template <typename TypeProvidedByStrategy, typename TypeRequestedByProxyNeeds>
    static void EnsureCanConvertFromTo()
    {
        static_assert(
            std::is_convertible<TypeProvidedByStrategy, TypeRequestedByProxyNeeds>::value,
            "Strategy does not provide a result type (-> `BaseProxy`) which is convertible to the one required by "
            "the `ProxyNeeds`. If you don't know how to debug this, go to this line and check the comment above.");
    }
};

}  // namespace score::mw::service::details

#endif  // SCORE_MW_SERVICE_DETAILS_PROXY_NEEDS_FACTORY_CHECKER_H
