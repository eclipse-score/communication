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

#include "score/containers/non_relocatable_vector.h"
#include "score/memory/data_type_size_info.h"
#include "score/mw/com/impl/generic_skeleton_event.h"
#include "score/mw/com/impl/methods/callable_traits.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/skeleton_field_base.h"
#include "score/result/result.h"
#include <score/span.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace score::mw::com::impl
{

/// \brief Represents a generic, type-erased skeleton field.
///
/// This class provides runtime-configurable field support for `GenericSkeleton`.
/// It manages notifications via an underlying `GenericSkeletonEvent` and serves
/// as a facade for field Get and Set operations based on the provided configuration.
class GenericSkeletonField : public SkeletonFieldBase
{
  public:
    /// \brief Production constructor that creates its underlying GenericSkeletonEvent internally.
    /// \param skeleton_base The parent skeleton instance.
    /// \param field_name The name of the field.
    /// \param size_info The data type size and alignment info for this field.
    /// \param has_getter Indicates whether this field is configured to have a Get handler.
    /// \param has_setter Indicates whether this field is configured to have a Set handler.
    GenericSkeletonField(SkeletonBase& skeleton_base,
                         const std::string_view field_name,
                         score::memory::DataTypeSizeInfo size_info,
                         bool has_getter,
                         bool has_setter);

    /// \brief Testing constructor that accepts an injected GenericSkeletonEvent directly.
    /// \param skeleton_base The parent skeleton instance.
    /// \param field_name The name of the field.
    /// \param size_info The data type size and alignment info for this field.
    /// \param generic_event The underlying type-erased event used for field notifications.
    /// \param has_getter Indicates whether this field is configured to have a Get handler.
    /// \param has_setter Indicates whether this field is configured to have a Set handler.
    GenericSkeletonField(SkeletonBase& skeleton_base,
                         const std::string_view field_name,
                         score::memory::DataTypeSizeInfo size_info,
                         std::unique_ptr<GenericSkeletonEvent> generic_event,
                         bool has_getter,
                         bool has_setter);

    ~GenericSkeletonField() override = default;
    GenericSkeletonField(const GenericSkeletonField&) = delete;
    GenericSkeletonField& operator=(const GenericSkeletonField&) & = delete;

    GenericSkeletonField(GenericSkeletonField&& other) noexcept = default;
    GenericSkeletonField& operator=(GenericSkeletonField&& other) & noexcept = default;

    /// \brief Updates the field value using a raw byte payload.
    /// \details If the field is configured with a getter, this updates the binding's
    ///          shared memory so the value is visible to subsequent Get requests.
    ///          If configured with a notifier, it also notifies subscribers.
    ///          If `OfferService()` has not been called yet, the payload is cached as
    ///          the initial value and the sending towards the binding deferred until the service is offered.
    /// \param raw_value The type-erased payload representing the new field value.
    [[nodiscard]] Result<void> Update(score::cpp::span<const uint8_t> raw_value);

    /// \brief Updates the field value using a pre-allocated sample.
    /// \details If the field is configured with a getter, this updates the binding's
    ///          shared memory so the value is visible to subsequent Get requests.
    ///          If configured with a notifier, it also notifies subscribers.
    /// \param sample A sample previously allocated via `Allocate()`.
    [[nodiscard]] Result<void> Update(SampleAllocateePtr<void> sample);

    /// \brief Allocates a zero-copy sample buffer for this field.
    /// \return The allocated sample pointer, or an error if allocation fails or
    ///         if OfferService() has not been called yet.
    [[nodiscard]] Result<SampleAllocateePtr<void>> Allocate();

    /// \brief Registers a handler for answering Set requests from proxies.
    /// \tparam CallableType Any callable (std::function, lambda, function pointer) with signature
    ///         void(score::cpp::span<uint8_t>).
    /// \param user_set_handler The user-provided handler callback.
    /// \return A result indicating success, or ComErrc::kCouldNotExecute if the field was not configured
    ///         with a setter (i.e. has_setter in the constructor was false).
    template <typename CallableType>
    [[nodiscard]] Result<void> RegisterSetHandler(CallableType&& /*user_set_handler*/)
    {
        using ActualCallableReturnType = typename get_callable_return_type<CallableType>::type;
        using ExpectedCallableReturnType = void;
        static_assert(std::is_same_v<ExpectedCallableReturnType, ActualCallableReturnType>,
                      "Registered set handler must have void return type!");

        using ActualCallableInArgType = typename get_callable_args_types<CallableType>::type;
        using ExpectedCallableInArgTypes = std::tuple<score::cpp::span<uint8_t>>;
        static_assert(std::is_same_v<ExpectedCallableInArgTypes, ActualCallableInArgType>,
                      "Registered set handler must take a single score::cpp::span<uint8_t> argument!");

        if (!has_setter_)
        {
            return MakeUnexpected(ComErrc::kCouldNotExecute);
        }
        score::mw::log::LogWarn("GenericSkeletonField") << "Setters are currently WIP and cannot be registered.";
        return MakeUnexpected(ComErrc::kCouldNotExecute);
    }

    /// \brief Internal hook to register the get handler with the binding.
    /// \details This is called by SkeletonFieldBase::PrepareOffer().
    /// \return A result indicating success or an error code.
    [[nodiscard]] Result<void> RegisterGetHandler() override;

    /// \brief Checks whether the underlying event binding was successfully created.
    [[nodiscard]] bool HasValidBinding() const noexcept;

  private:
    /// \brief Checks if a valid initial value was cached prior to offering the service.
    bool IsInitialValueSaved() const noexcept override;
    /// \brief Allocates and sends the cached initial value after the service is successfully offered.
    [[nodiscard]] Result<void> DoDeferredUpdate() noexcept override;
    [[nodiscard]] bool IsSetHandlerMissing() const noexcept override;

    [[nodiscard]] Result<void> UpdateImpl(score::cpp::span<const uint8_t> raw_value);

    static std::unique_ptr<GenericSkeletonEvent> MakeGenericEvent(SkeletonBase& skeleton_base,
                                                                  const std::string_view field_name,
                                                                  const score::memory::DataTypeSizeInfo size_info);

    GenericSkeletonEvent* GetGenericEvent() const;

    score::containers::NonRelocatableVector<uint8_t> initial_field_value_;
    bool has_initial_value_;
    bool has_getter_;
    bool has_setter_;
    bool is_set_handler_registered_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_FIELD_H_
