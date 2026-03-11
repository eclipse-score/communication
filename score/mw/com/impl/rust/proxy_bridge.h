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
#ifndef SCORE_MW_COM_IMPL_RUST_PROXY_BRIDGE_H
#define SCORE_MW_COM_IMPL_RUST_PROXY_BRIDGE_H

#include <score/assert.hpp>

#include <utility>

namespace score::mw::com::impl::rust
{

struct FatPtr
{
    const void* vtbl;
    void* data;
};

template <typename R, typename... Args>
class RustBoxedCallable;

template <typename R, typename... Args>
class RustRefMutCallable;

template <template <typename, typename...> class FnMutHandler, typename R, typename... Args>
class RustFnMutCallableBase
{
  public:
    using Handler = FnMutHandler<R, Args...>;

    /// Constructs a new callable from a Rust fat ptr that was created from a Box<dyn FnMut()>.
    ///
    /// Any other type will lead to UB as the used Rust callback functions will assume this and transmute into that type
    // to call the callback on Rust side.
    explicit RustFnMutCallableBase(const FatPtr& dyn_fnmut) noexcept : ptr_{dyn_fnmut}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(dyn_fnmut.data != nullptr,
                               "Failed creating a RustFnMutCallable due to an invalid pointer");
    }

    RustFnMutCallableBase(const RustFnMutCallableBase&) = delete;
    RustFnMutCallableBase(RustFnMutCallableBase&& other) noexcept : ptr_{other.ptr_}
    {
        other.ptr_.data = nullptr;
    }

    RustFnMutCallableBase& operator=(const RustFnMutCallableBase&) = delete;
    RustFnMutCallableBase& operator=(RustFnMutCallableBase&& other) noexcept
    {
        std::swap(ptr_, other.ptr_);
        return *this;
    }

    ~RustFnMutCallableBase() noexcept
    {
        if (ptr_.data != nullptr)
        {
            Handler::dispose(ptr_);
        }
    }

  protected:
    void CheckValid() const noexcept
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(ptr_.data != nullptr, "Tried to use a Rust FnMut callable without a valid pointer");
    }

    FatPtr ptr_;
};

template <template <typename, typename...> class FnMutHandler, typename R = void, typename... Args>
class RustFnMutCallable : public RustFnMutCallableBase<FnMutHandler, R, Args...>
{
  public:
    using Base = RustFnMutCallableBase<FnMutHandler, R, Args...>;
    using RustFnMutCallableBase<FnMutHandler, R, Args...>::RustFnMutCallableBase;

    template <typename... CallArgs>
    R operator()(CallArgs&&... call_args) noexcept
    {
        Base::CheckValid();
        return Base::Handler::invoke(this->ptr_, std::forward<CallArgs>(call_args)...);
    }

    template <typename... CallArgs>
    R operator()(CallArgs&&... call_args) const noexcept
    {
        Base::CheckValid();
        return Base::Handler::invoke(this->ptr_, std::forward<CallArgs>(call_args)...);
    }
};

template <template <typename, typename...> class FnMutHandler, typename... Args>
class RustFnMutCallable<FnMutHandler, void, Args...> final : public RustFnMutCallableBase<FnMutHandler, void, Args...>
{
  public:
    using Base = RustFnMutCallableBase<FnMutHandler, void, Args...>;
    using RustFnMutCallableBase<FnMutHandler, void, Args...>::RustFnMutCallableBase;

    template <typename... CallArgs>
    void operator()(CallArgs&&... call_args) noexcept
    {
        Base::CheckValid();
        Base::Handler::invoke(this->ptr_, std::forward<CallArgs>(call_args)...);
    }

    template <typename... CallArgs>
    void operator()(CallArgs&&... call_args) const noexcept
    {
        Base::CheckValid();
        Base::Handler::invoke(this->ptr_, std::forward<CallArgs>(call_args)...);
    }
};

}  // namespace score::mw::com::impl::rust

#endif  // SCORE_MW_COM_IMPL_RUST_PROXY_BRIDGE_H
