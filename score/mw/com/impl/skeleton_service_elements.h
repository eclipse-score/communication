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
#ifndef SCORE_MW_COM_IMPL_SKELETON_SERVICE_ELEMENTS_H
#define SCORE_MW_COM_IMPL_SKELETON_SERVICE_ELEMENTS_H

#include <functional>
#include <map>
#include <string_view>

namespace score::mw::com::impl
{

// Forward declarations — full definitions are included only by skeleton_service_elements.cpp.
class SkeletonEventBase;
class SkeletonFieldBase;
class SkeletonMethodBase;

/// \brief Encapsulates the three service-element registries (events, fields, methods) of a SkeletonBase
///        together with the post-move reference-fixup logic.
///
/// \details SkeletonBase previously stored three std::map members directly and duplicated
///          the same three UpdateSkeletonReference loops in both the move constructor and the
///          move assignment operator.  This class extracts that ownership and logic so that:
///
///          1. All map management (Register / Update / Get) is in one place.
///          2. The reference-fixup loop (UpdateSkeletonReferences) is declared once.
///          3. SkeletonBase's move constructor and move assignment operator each only need to
///             call  service_elements_.UpdateSkeletonReferences(*this)  after delegating the
///             member-move to the compiler-generated default move of this class.
///
/// The move constructor and move assignment operator of this class are intentionally defaulted:
/// they simply move the three maps.  The caller (SkeletonBase) is responsible for calling
/// UpdateSkeletonReferences on the new owner after the move, because the correct new-owner
/// address is only available at the SkeletonBase level.
class SkeletonServiceElements
{
  public:
    using SkeletonEvents = std::map<std::string_view, std::reference_wrapper<SkeletonEventBase>>;
    using SkeletonFields = std::map<std::string_view, std::reference_wrapper<SkeletonFieldBase>>;
    using SkeletonMethods = std::map<std::string_view, std::reference_wrapper<SkeletonMethodBase>>;

    SkeletonServiceElements() = default;
    ~SkeletonServiceElements() = default;

    /// \brief Not copyable — mirrors SkeletonBase's copy-deletion requirement.
    SkeletonServiceElements(const SkeletonServiceElements&) = delete;
    SkeletonServiceElements& operator=(const SkeletonServiceElements&) = delete;

    /// \brief Default move: moves the three maps; does NOT update skeleton back-references.
    /// The owning SkeletonBase must call UpdateSkeletonReferences(*this) after the move.
    SkeletonServiceElements(SkeletonServiceElements&&) noexcept = default;
    SkeletonServiceElements& operator=(SkeletonServiceElements&&) noexcept = default;

    // ── Registry operations ───────────────────────────────────────────────────

    /// \brief Register an event under the given name.  Asserts that the name is unique.
    void RegisterEvent(std::string_view event_name, SkeletonEventBase& event) noexcept;

    /// \brief Register a field under the given name.  Asserts that the name is unique.
    void RegisterField(std::string_view field_name, SkeletonFieldBase& field) noexcept;

    /// \brief Register a method under the given name.  Asserts that the name is unique.
    void RegisterMethod(std::string_view method_name, SkeletonMethodBase& method) noexcept;

    /// \brief Update the stored reference for an existing event (used after move construction
    ///        of a generated Skeleton whose event members have new addresses).
    void UpdateEvent(std::string_view event_name, SkeletonEventBase& event) noexcept;

    /// \brief Update the stored reference for an existing field.
    void UpdateField(std::string_view field_name, SkeletonFieldBase& field) noexcept;

    /// \brief Update the stored reference for an existing method.
    void UpdateMethod(std::string_view method_name, SkeletonMethodBase& method) noexcept;

    // ── Accessors ─────────────────────────────────────────────────────────────

    const SkeletonEvents& GetEvents() const noexcept
    {
        return events_;
    }
    const SkeletonFields& GetFields() const noexcept
    {
        return fields_;
    }
    const SkeletonMethods& GetMethods() const noexcept
    {
        return methods_;
    }

  private:
    SkeletonEvents events_{};
    SkeletonFields fields_{};
    SkeletonMethods methods_{};
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_SERVICE_ELEMENTS_H
