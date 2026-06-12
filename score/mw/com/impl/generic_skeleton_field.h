/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_GENERIC_SKELETON_FIELD_H_
#define SCORE_MW_COM_IMPL_GENERIC_SKELETON_FIELD_H_

#include "score/mw/com/impl/generic_skeleton_event.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/skeleton_field_base.h"
#include "score/result/result.h"
#include <score/span.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace score::mw::com::impl
{

/// @brief Represents a generic, type-erased skeleton field.
///
/// This class provides runtime-configurable field support for `GenericSkeleton`.
/// It manages notifications via an underlying `GenericSkeletonEvent` and serves
/// as a facade for field Get and Set operations based on the provided configuration.
class GenericSkeletonField : public SkeletonFieldBase
{
  public:
    /// @brief Constructs a new GenericSkeletonField.
    /// @param skeleton_base The parent skeleton instance.
    /// @param field_name The logical name of the field.
    /// @param generic_event The underlying type-erased event used for field notifications.
    /// @param has_getter Indicates whether this field is configured to have a Get handler.
    /// @param has_setter Indicates whether this field is configured to have a Set handler.
    /// @param has_notifier Indicates whether this field is configured to support notifications.
    GenericSkeletonField(SkeletonBase& skeleton_base,
                         const std::string_view field_name,
                         std::unique_ptr<GenericSkeletonEvent> generic_event,
                         bool has_getter,
                         bool has_setter,
                         bool has_notifier);

    ~GenericSkeletonField() override = default;
    GenericSkeletonField(const GenericSkeletonField&) = delete;
    GenericSkeletonField& operator=(const GenericSkeletonField&) & = delete;

    GenericSkeletonField(GenericSkeletonField&& other) noexcept;
    GenericSkeletonField& operator=(GenericSkeletonField&& other) & noexcept;

    /// @brief Updates the reference to the parent skeleton when the skeleton is moved.
    void UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept override;

    /// @brief Updates the field value using a raw byte payload and notifies subscribers.
    /// @details If `OfferService()` has not been called yet, the payload is cached as
    ///          the initial value and deferred until the service is offered.
    /// @param raw_value The type-erased payload representing the new field value.
    /// @return A result indicating success or an error code on failure.
    Result<void> Update(score::cpp::span<const uint8_t> raw_value) noexcept;

    /// @brief Updates the field value using a pre-allocated sample and notifies subscribers.
    /// @param sample A sample previously allocated via `Allocate()`.
    /// @return A result indicating success or an error code on failure.
    Result<void> Update(SampleAllocateePtr<void> sample) noexcept;

    /// @brief Allocates a zero-copy sample buffer for this field.
    /// @return The allocated sample pointer, or an error if allocation fails or
    ///         if the field does not support notifications.
    Result<SampleAllocateePtr<void>> Allocate() noexcept;

    /// @brief Registers a handler for answering Get requests from proxies.
    /// @param get_handler The user-provided handler callback returning the field value as raw bytes.
    /// @return A result indicating success or an error code.
    Result<void> RegisterGetHandler(std::function<std::vector<uint8_t>()> get_handler);

    /// @brief Registers a handler for answering Set requests from proxies.
    /// @param set_handler The user-provided handler callback accepting raw bytes and returning
    ///        the accepted/modified field value as raw bytes.
    /// @return A result indicating success or an error code.
    Result<void> RegisterSetHandler(std::function<std::vector<uint8_t>(score::cpp::span<const uint8_t>)> set_handler);

  private:
    /// @brief Checks if a valid initial value was cached prior to offering the service.
    bool IsInitialValueSaved() const noexcept override;
    /// @brief Allocates and sends the cached initial value after the service is successfully offered.
    Result<void> DoDeferredUpdate() noexcept override;
    [[nodiscard]] bool IsSetHandlerMissing() const noexcept override;

    GenericSkeletonEvent* GetGenericEvent() const noexcept;

    std::vector<uint8_t> initial_field_value_;
    bool has_initial_value_{false};

    bool has_getter_{false};
    bool has_setter_{false};
    bool has_notifier_{false};
    bool is_set_handler_registered_{false};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_FIELD_H_
