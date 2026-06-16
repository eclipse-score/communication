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

#include <memory>

namespace score::mw::com::impl
{

/// \brief Allows moveable classes to hand out references to themselves while allowing these references to be updated
/// when the moveable class T is moved.
///
/// Moveable class, T, will store a ReferenceToMoveable<T> and hand out references to a
/// ReferenceToMoveable<T>::Reference (instead of pointers to itself) via Get. The ReferenceToMoveable::Reference is
/// stored on the heap and will never be moved. The class T must manually update the reference to itself via Update in
/// this class when moved.
///
/// See the test containing DummyClass for an example of how to use this class together with
/// ReferenceToMoveable<T>::Reference.
///
/// Reason for this class: In various places in our codebase, we have the issue in which we hand out a reference to a
/// class which is moveable. If the class is moved, then we need to update this reference. E.g. SkeletonBase stores a
/// map of references to its service elements. Each service element therefore must store a reference to the SkeletonBase
/// and update the reference to itself when it's moved. The SkeletonBase must also update the reference to itself within
/// each service element when it is moved.
///
/// To avoid such complexity, this commit introduces a class ReferenceToMoveable::Reference which can be
/// given to the SkeletonBase which is stored in the heap so a reference to it will never be invalidated. This then
/// stores a reference to the service element which is updated by the service element if it's moved. With this approach,
/// the service element doesn't need to store a reference to the parent SkeletonBase so the parent SkeletonBase doesn't
/// need to inform the service element when it moves.
///
/// NOTE: This class is not thread safe. It is the responsibility of the user to ensure that the reference is updated
/// before it is accessed after a move. In our current architecture, this is not an issue as we only move our service
/// elements when the user Skeleton / Proxy object is moved which owns the service elements. So the user cannot be
/// accessing the service elements while they are being moved.
template <typename T>
class ReferenceToMoveable
{
  public:
    class Reference
    {
        friend class ReferenceToMoveable;

        struct PrivateConstructorTag
        {
        };

      public:
        Reference(PrivateConstructorTag, T& reference_object) : reference_object_{reference_object} {}

        Reference(const Reference&) = delete;
        Reference& operator=(const Reference&) = delete;
        Reference(Reference&&) = delete;
        Reference& operator=(Reference&&) = delete;
        ~Reference() = default;

        const T& Get() const
        {
            return reference_object_.get();
        }

        T& Get()
        {
            return reference_object_.get();
        }

      private:
        void Update(T& field)
        {
            reference_object_ = field;
        }

        std::reference_wrapper<T> reference_object_;
    };

    explicit ReferenceToMoveable(T& field)
        : reference_to_moveable_{std::make_unique<Reference>(typename Reference::PrivateConstructorTag{}, field)}
    {
    }

    Reference& Get()
    {
        return *(reference_to_moveable_);
    }

    const Reference& Get() const
    {
        return *(reference_to_moveable_);
    }

    void Update(T& field)
    {
        reference_to_moveable_->Update(field);
    }

  private:
    std::unique_ptr<Reference> reference_to_moveable_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_REFERENCE_TO_MOVEABLE_H
