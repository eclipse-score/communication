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

#ifndef SCORE_MW_SERVICE_DETAILS_CREATE_PROXIES_H
#define SCORE_MW_SERVICE_DETAILS_CREATE_PROXIES_H

#include "score/mw/service/details/proxy_spec_traits.h"
#include "score/mw/service/proxy_builder_base.h"

#include "score/stop_token.hpp"
#include "score/utility.hpp"

#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>

namespace score::mw::service::details
{

/// @brief This is a helper class to create a respective builder for each ProxySpec. This is needed, since the `Build()`
/// function within the ProxySpecTraits is different, depending if the ProxySpec is a _mandatory_ or _optional_ proxy.
/// For that purpose we have to combine the `ProxySpec` with the tuple of builders. Thus, we need some kind of zip
/// functionality.
///
/// @note This class is unit-tested through proxy_needs_test.cpp. Due to high coupling with its logic
///
/// @tparam ProxySpec The definition of which kind of proxies are needed
template <typename... ProxySpec>
class ProxyCreator
{
  public:
    constexpr ProxyCreator() noexcept = delete;

    template <template <typename...> class Tuple, typename... TupleElements>
    static auto RequestProxies(std::optional<score::cpp::stop_token> token, Tuple<TupleElements...> builders)
    {
        return RequestProxiesIterateViaFold(
            std::move(token), std::move(builders), std::make_index_sequence<sizeof...(TupleElements)>{});
    }

    template <template <typename...> class Tuple, typename... TupleElements>
    static auto GetProxies(score::cpp::stop_token token, Tuple<TupleElements...> builder_results)
    {
        return GetProxiesIterateViaFold(
            std::move(token), std::move(builder_results), std::make_index_sequence<sizeof...(TupleElements)>{});
    }

    template <template <typename...> class Tuple, typename... TupleElements>
    static auto GetProxies(Tuple<TupleElements...> builder_results)
    {
        return GetProxiesIterateViaFold(std::move(builder_results),
                                        std::make_index_sequence<sizeof...(TupleElements)>{});
    }

  private:
    // Helper function to iterate through tuple via fold expression
    template <template <typename...> class Tuple, typename... TupleElements, std::size_t... index>
    static auto RequestProxiesIterateViaFold(std::optional<score::cpp::stop_token> token,
                                             Tuple<TupleElements...> builders,
                                             const std::index_sequence<index...> index_seq)
    {
        score::cpp::ignore = index_seq;
        return std::make_tuple(ProxySpecTraits<ProxySpec>::Build(*std::get<index>(builders), token)...);
    }

    // Helper function to iterate through tuple via fold expression
    template <template <typename...> class Tuple, typename... TupleElements, std::size_t... index>
    static auto GetProxiesIterateViaFold(score::cpp::stop_token token,
                                         Tuple<TupleElements...> builder_results,
                                         const std::index_sequence<index...> index_seq)
    {
        score::cpp::ignore = index_seq;
        return std::make_tuple(ProxySpecTraits<ProxySpec>::Get(std::move(std::get<index>(builder_results)), token)...);
    }

    // Helper function to iterate through tuple via fold expression
    template <template <typename...> class Tuple, typename... TupleElements, std::size_t... index>
    static auto GetProxiesIterateViaFold(Tuple<TupleElements...> builder_results,
                                         const std::index_sequence<index...> index_seq)
    {
        score::cpp::ignore = index_seq;
        return std::make_tuple(ProxySpecTraits<ProxySpec>::Get(std::move(std::get<index>(builder_results)))...);
    }
};

}  // namespace score::mw::service::details

#endif  // SCORE_MW_SERVICE_DETAILS_CREATE_PROXIES_H
