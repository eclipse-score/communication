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
#ifndef SCORE_MW_COM_IMPL_ENABLE_REFERENCE_TO_MOVEABLE_FROM_THIS_H
#define SCORE_MW_COM_IMPL_ENABLE_REFERENCE_TO_MOVEABLE_FROM_THIS_H

#include "score/mw/com/impl/reference_to_moveable.h"

namespace score::mw::com::impl
{

/// \brief Class that should be inherited by moveable class T to allow it to hand out references to itself while
/// allowing these references to be updated when the moveable class T is moved.
//
/// See the test file for this class for an example of how to use this class.
///
/// Reason for this class: In various places in our codebase, we have the issue in which we hand out a reference to a
/// class which is moveable. If the class is moved, then we need to update this reference. E.g. SkeletonBase stores a
/// map of references to its service elements. Each service element therefore must store a reference to the SkeletonBase
/// and update the reference to itself when it's moved. The SkeletonBase must also update the reference to itself within
/// each service element when it is moved.
///
/// To avoid such complexity, this commit introduces a class EnableReferenceToMoveableFromThis which will be inherited
/// by each service element. This stores a ReferenceToMoveable which is stored on the heap so a reference to it will
/// never be invalidated. The ReferenceToMoveable will be updated when the service element is moved. A
/// ReferenceToMoveable::Reference is provided to SkeletonBase from which it can get a reference to the service element,
/// even if it's been moved. With this approach, the service element doesn't need to store a reference to the parent
/// SkeletonBase so the parent SkeletonBase doesn't need to inform the service element when it moves.
///
/// NOTE: This class is not thread safe. It is the responsibility of the user to ensure that the reference is not
/// accessed while the moveable class (and therefore this class) is being moved. In our current architecture, this is
/// not an issue as we only allow single threaded access to Skeleton / Proxy objects and their service elements, so we
/// will never be accessing the references while a Skeleton or Proxy object is being moved.
template <typename T>
class EnableReferenceToMoveableFromThis
{
  public:
    EnableReferenceToMoveableFromThis() : ptr_wrapper_{*this} {}

    virtual ~EnableReferenceToMoveableFromThis() = default;

    EnableReferenceToMoveableFromThis(EnableReferenceToMoveableFromThis& other) noexcept = delete;
    EnableReferenceToMoveableFromThis& operator=(EnableReferenceToMoveableFromThis& other) & noexcept = delete;

    EnableReferenceToMoveableFromThis(EnableReferenceToMoveableFromThis&& other) noexcept
        : ptr_wrapper_{std::move(other.ptr_wrapper_)}
    {
        ptr_wrapper_.Update(*this);
    }
    EnableReferenceToMoveableFromThis& operator=(EnableReferenceToMoveableFromThis&& other) & noexcept
    {
        if (this != &other)
        {
            ptr_wrapper_ = std::move(other.ptr_wrapper_);
            ptr_wrapper_.Update(*this);
        }
        return *this;
    }

    typename ReferenceToMoveable<T>::Reference& GetReferenceToMoveable()
    {
        return ptr_wrapper_.Get();
    }

  private:
    ReferenceToMoveable<T> ptr_wrapper_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_ENABLE_REFERENCE_TO_MOVEABLE_FROM_THIS_H
