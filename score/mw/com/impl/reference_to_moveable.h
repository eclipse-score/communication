/********************************************************************************
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
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_REFERENCE_TO_MOVEABLE_H
#define SCORE_MW_COM_IMPL_REFERENCE_TO_MOVEABLE_H

#include <score/assert.hpp>

#include <memory>

namespace score::mw::com::impl
{

template <typename T>
class EnableReferenceToMoveableFromThis;

/// \brief Allows moveable classes to hand out references to themselves while allowing these references to be updated
/// when the moveable class T is moved.
///
/// This class in intended to be used in conjunction with EnableReferenceToMoveableFromThis. A moveable class T should
/// inherit from EnableReferenceToMoveableFromThis<T> and then hand out references to itself via
/// GetReferenceToMoveable().
///
/// NOTE: This class is not thread safe. It is the responsibility of the user to ensure that the reference is updated
/// before it is accessed after a move. In our current architecture, this is not an issue as we only move our service
/// elements when the user Skeleton / Proxy object is moved which owns the service elements. So the user cannot be
/// accessing the service elements while they are being moved.
template <typename T>
class ReferenceToMoveable
{
    friend class EnableReferenceToMoveableFromThis<T>;

  public:
    /// \brief Class which is handed out to other classes which need a reference to the moveable class T via
    /// ReferenceToMoveable::Get().
    ///
    /// This class is stored on the heap and will never be moved so references to it will never be invalidated. We need
    /// this class instead of handing out a reference to a reference_wrapper stored on the heap to make it clear in the
    /// calling code that the reference will never be invalidated and to ensure that it can only be created by
    /// ReferenceToMoveable.
    ///
    /// E.g. so that the calling code will look like this:
    ///     using SkeletonEvents = std::map<std::string_view,
    ///         std::reference_wrapper<ReferenceToMoveable<SkeletonEventBase>::Reference>>;
    ///
    /// and not like this:
    ///     using SkeletonEvents = std::map<std::string_view,
    ///         std::reference_wrapper<std::reference_wrapper<SkeletonEventBase>>>;
    class Reference
    {
        friend class ReferenceToMoveable;

        struct PrivateConstructorTag
        {
        };

      public:
        Reference(PrivateConstructorTag, EnableReferenceToMoveableFromThis<T>& reference_object)
            : reference_object_{reference_object}
        {
        }

        ~Reference() = default;

        Reference(const Reference&) = delete;
        Reference& operator=(const Reference&) = delete;
        Reference(Reference&&) = delete;
        Reference& operator=(Reference&&) = delete;

        /// \brief Returns a reference to the moveable class T.
        ///
        /// Since reference_object_ points to the EnableReferenceToMoveableFromThis<T> which moveable class T inherits
        /// from, we can static_cast it to a T&.
        ///
        /// The returned reference will _NOT_ be updated when the moveable class T is moved. Get() must be recalled
        /// after the moveable class T is moved. Therefore, a class should always store a reference to
        /// ReferenceToMoveable::Reference and call Get() on it whenever it needs to access the moveable class T.
        ///
        /// Since T& Get() must not be called on a temporary object (since the returned reference would be dangling), it
        /// must have a trailing & in the signature. const T& Get() is allowed to be called on a temporary object since
        /// the returned reference will not be dangling (since the const ref will extend the lifetime of the temporary
        /// object). We cannot overload 'T& Get() &' with 'const T& Get() const', so we must explicitly provide const T&
        /// Get() const& and const T& Get() const&& overloads.
        const T& Get() const&
        {
            const auto& original_ref = reference_object_.get();
            return static_cast<const T&>(original_ref);
        }

        const T& Get() const&&
        {
            const auto& original_ref = reference_object_.get();
            return static_cast<const T&>(original_ref);
        }

        T& Get() &
        {
            auto& original_ref = reference_object_.get();
            return static_cast<T&>(original_ref);
        }

      private:
        void Update(EnableReferenceToMoveableFromThis<T>& field)
        {
            reference_object_ = field;
        }

        std::reference_wrapper<EnableReferenceToMoveableFromThis<T>> reference_object_;
    };

    // See explanation above Reference::Get() for why we need const Reference& Get() const& and const Reference& Get()
    // const&& overloads.
    const Reference& Get() const&
    {
        return *(reference_to_moveable_);
    }

    const Reference& Get() const&&
    {
        return *(reference_to_moveable_);
    }

    Reference& Get() &
    {
        return *(reference_to_moveable_);
    }

    void Update(EnableReferenceToMoveableFromThis<T>& field)
    {
        reference_to_moveable_->Update(field);
    }

  private:
    explicit ReferenceToMoveable(EnableReferenceToMoveableFromThis<T>& field)
        : reference_to_moveable_{std::make_unique<Reference>(typename Reference::PrivateConstructorTag{}, field)}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(reference_to_moveable_ != nullptr);
    }

    std::unique_ptr<Reference> reference_to_moveable_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_REFERENCE_TO_MOVEABLE_H
