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
#ifndef SCORE_MW_COM_TEST_METHODS_RUNTIME_SIZED_ARRAY_H
#define SCORE_MW_COM_TEST_METHODS_RUNTIME_SIZED_ARRAY_H

#include <score/assert.hpp>

#include <memory>
#include <type_traits>

namespace score::mw::com::test
{
namespace detail
{

template <class T>
constexpr T* to_address(T* p)
{
    static_assert(!std::is_function_v<T>);
    return p;
}

template <class T>
constexpr auto to_address(const T& p)
{
    return to_address(p.operator->());
}

}  // namespace detail

template <typename ElementType, typename Allocator = std::allocator<ElementType>>
class RuntimeSizedArray
{
  public:
    using value_type = ElementType;
    using reference = ElementType&;
    using const_reference = const ElementType&;
    using iterator = ElementType*;
    using const_iterator = const ElementType*;
    using difference_type =
        typename std::pointer_traits<typename std::allocator_traits<Allocator>::pointer>::difference_type;
    using size_type = std::size_t;

    explicit RuntimeSizedArray(const Allocator& alloc) : alloc_{std::move(alloc)} {}

    ~RuntimeSizedArray();

    RuntimeSizedArray(const RuntimeSizedArray&) = delete;
    RuntimeSizedArray& operator=(const RuntimeSizedArray&) = delete;
    RuntimeSizedArray(RuntimeSizedArray&&) = delete;
    RuntimeSizedArray& operator=(RuntimeSizedArray&&) = delete;

    void ReserveOnce(size_type new_capacity);

    template <typename... Args>
    auto emplace_back(Args&&... args) -> ElementType&;

    ElementType& at(const size_type index);
    const ElementType& at(const size_type index) const;

    size_type size() const noexcept
    {
        return size_;
    }

    size_type capacity() const noexcept
    {
        return capacity_;
    }

    const value_type* data() const
    {
        return data_;
    }

  private:
    using pointer = typename std::allocator_traits<Allocator>::pointer;

    pointer allocate_array(size_type number_of_elements, Allocator& allocator);

    typename std::allocator_traits<Allocator>::pointer data_{nullptr};
    Allocator alloc_;
    size_type size_{0U};
    size_type capacity_{0U};
};

template <typename ElementType, typename Allocator>
RuntimeSizedArray<ElementType, Allocator>::~RuntimeSizedArray()
{
    if (data_ != nullptr)
    {
        std::allocator_traits<Allocator>::deallocate(alloc_, data_, capacity_);
    }
}

template <typename ElementType, typename Allocator>
auto RuntimeSizedArray<ElementType, Allocator>::allocate_array(size_type number_of_elements, Allocator& allocator)
    -> pointer
{
    auto storage = std::allocator_traits<Allocator>::allocate(allocator, number_of_elements);
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(nullptr != storage, "no memory allocated");
    return storage;
}

template <typename ElementType, typename Allocator>
void RuntimeSizedArray<ElementType, Allocator>::ReserveOnce(const size_type new_capacity)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(new_capacity > 0, "new_capacity must be > 0");
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(capacity_ == 0, "ReserveOnce() can only be called once");
    data_ = allocate_array(new_capacity, alloc_);
    capacity_ = new_capacity;
}

template <typename ElementType, typename Allocator>
template <typename... Args>
auto RuntimeSizedArray<ElementType, Allocator>::emplace_back(Args&&... args) -> ElementType&
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        size_ < capacity_,
        "Capacity of vector set in constructor has already been reached. Cannot emplace another element.");

    auto* current_storage_pointer = detail::to_address(data_);
    std::advance(current_storage_pointer, static_cast<difference_type>(size_));
    std::allocator_traits<Allocator>::construct(alloc_, current_storage_pointer, std::forward<Args>(args)...);
    size_ += 1U;
    return *current_storage_pointer;
}

template <typename ElementType, typename Allocator>
inline ElementType& RuntimeSizedArray<ElementType, Allocator>::at(const size_type index)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(index < size_, "index out of bounds");
    return *(std::next(data_, static_cast<difference_type>(index)));
}

template <typename ElementType, typename Allocator>
inline const ElementType& RuntimeSizedArray<ElementType, Allocator>::at(const size_type index) const
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(index < size_, "index out of bounds");
    return *(std::next(data_, static_cast<difference_type>(index)));
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_RUNTIME_SIZED_ARRAY_H
