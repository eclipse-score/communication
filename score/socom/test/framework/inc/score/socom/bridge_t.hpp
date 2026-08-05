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

#ifndef SRC_SOCOM_TEST_FRAMEWORK_INC_SCORE_SOCOM_BRIDGE_T
#define SRC_SOCOM_TEST_FRAMEWORK_INC_SCORE_SOCOM_BRIDGE_T

#include <atomic>
#include <optional>

#include "score/socom/connector_factory.hpp"
#include "score/socom/socom_mocks.hpp"

namespace score::socom {

/// \brief Facade for simple bridge behaviour and callbacks in tests
///
/// It allows easy configuration of mocks and blocks its destruction until all
/// expectations have been fulfilled.
class Bridge_data {
   public:
    /// \brief Expect which callbacks will be called and thus need to be configured
    enum Expect { nothing, request_service_function };

   private:
    Bridge_identity m_identity{Bridge_identity::make(*this)};
    Request_service_function_mock m_rsf_mock;

    std::atomic<bool> m_request_find_service_created{false};
    std::atomic<bool> m_request_find_service_destroyed{true};

    Service_bridge_registration m_bridge_registration{nullptr};

    ::score::Result<Service_bridge_registration> register_at_runtime(
        Connector_factory& connector_factory);

   public:
    /// \brief Set order of bridge callback registration and callback configuration.
    ///
    /// When configuring callbacks it needs to be known if there are already clients registered to
    /// the runtime
    enum Creation_sequence { bridge_then_expect, expect_then_bridge };

    /// \brief Create new Bridge facade
    ///
    /// \param[in] sequence whether to register or configure callbacks first
    /// \param[in] expect which callbacks will be called
    /// \param[in] connector_factory the runtime facade to register to
    /// \param[in] ctor_callback will be called from constructor
    Bridge_data(
        Creation_sequence const& sequence, Expect const& expect,
        Connector_factory& connector_factory,
        std::function<void(Bridge_data&)>&& ctor_callback = [](Bridge_data& /*unused*/) {});

    Bridge_data(Bridge_data const&) = delete;
    Bridge_data(Bridge_data&&) = delete;

    ~Bridge_data();

    Bridge_data& operator=(Bridge_data const&) = delete;
    Bridge_data& operator=(Bridge_data&&) = delete;

    /// \brief Expect callbacks to be called
    ///
    /// \param[in] expect which callbacks will be called
    /// \param[in] connector_factory container of configuration and instance id
    void expect_callbacks(Expect const& expect, Connector_factory const& connector_factory);

    /// \brief Allow the Bridge_data instance to be destroyed before the expectations of the
    /// callbacks have been fulfilled
    void no_destroyed_check();

    /// \brief Expect call to request_find_service()
    ///
    /// \param[in] configuration
    /// \param[in] instance
    /// \param[in] rsf Function to call when configured request_find_service() is called
    void expect_request_find_service(
        Service_interface_definition const& configuration, Service_instance const& instance,
        std::function<void(Service_interface_definition const&, Service_instance const&)>&& rsf =
            [](auto const& /*configuration*/, auto const& /*instance*/) {});

    /// \return atomic to check if runtime called request_find_service
    std::atomic<bool> const& get_request_find_service_created() const;

    /// \return atomic to check if request_find_service handle was destroyed by the runtime
    std::atomic<bool> const& get_request_find_service_destroyed() const;

    std::optional<Bridge_identity> get_identity() const;
};

}  // namespace score::socom

#endif  // SRC_SOCOM_TEST_FRAMEWORK_INC_SCORE_SOCOM_BRIDGE_T
