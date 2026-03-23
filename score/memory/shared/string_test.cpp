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
#include "score/memory/shared/string.h"

#include "score/memory/shared/fake/my_memory_resource.h"

#include <gtest/gtest.h>

#include <score/string_view.hpp>

namespace score::memory::shared
{
namespace
{

constexpr const char lorem_ipsum_str[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore "
    "magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo "
    "consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. "
    "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
constexpr score::cpp::string_view lorem_ipsum{lorem_ipsum_str, sizeof(lorem_ipsum_str)};

TEST(StringTest, StringUsesProvidedMemoryResource)
{
    // Given a string that is associated with our memory resource
    test::MyMemoryResource memory{};
    PolymorphicOffsetPtrAllocator<String> allocator{memory};
    String unit{allocator};
    EXPECT_EQ(memory.getAllocatedMemory(), 0U);  // A default-constructed string shall not allocate any data yet.

    // When assigning some test string
    unit = lorem_ipsum.data();

    // Then the memory is allocated on our provided memory resource
    EXPECT_GE(memory.getAllocatedMemory(), lorem_ipsum.size());
}

TEST(StringTest, CompareStringToStdString)
{
    test::MyMemoryResource memory{};
    PolymorphicOffsetPtrAllocator<String> allocator{memory};
    String my_string{"OÖKuzidaskjiksoaddszfkjdfdskjkjdskmlkjdnfmgbjhtknfgbiuhte", allocator};
    const std::size_t after_first_allocation{memory.getAllocatedMemory()};
    EXPECT_GT(after_first_allocation, 0U);
    std::string std_string{"JKLgfkdlsjfosflöewjhrlkghb,öärtm,fgplkrejwhrizfewgwuzklmdas,löfds"};
    EXPECT_EQ(memory.getAllocatedMemory(), after_first_allocation);
    String equal_string{std_string.data(), std_string.size(), allocator};
    EXPECT_GT(memory.getAllocatedMemory(), after_first_allocation);

    EXPECT_TRUE(my_string != std_string);
    EXPECT_FALSE(my_string == std_string);
    EXPECT_TRUE(std_string != my_string);
    EXPECT_FALSE(std_string == my_string);
    EXPECT_TRUE(my_string == my_string);
    EXPECT_FALSE(my_string != my_string);
    EXPECT_TRUE(equal_string == std_string);
    EXPECT_FALSE(equal_string != std_string);
}

TEST(StringTest, OutputOperatorOverload)
{
    test::MyMemoryResource memory{};
    PolymorphicOffsetPtrAllocator<String> allocator{memory};
    String my_string{"OÖKuzidaskjiksoaddszfkjdfdskjkjdskmlkjdnfmgbjhtknfgbiuhte", allocator};

    std::ostringstream out_stream;
    out_stream << my_string;

    EXPECT_STREQ(my_string.c_str(), out_stream.str().c_str());
}

TEST(StringTest, InputOperatorOverload)
{
    test::MyMemoryResource memory{};
    PolymorphicOffsetPtrAllocator<String> allocator{memory};
    String my_string{"", allocator};

    const char* const test_string{"OÖKuzidaskjiksoaddszfkjdfdskjkjdskmlkjdnfmgbjhtknfgbiuhte"};

    std::istringstream in_stream{test_string};

    in_stream >> my_string;

    EXPECT_STREQ(my_string.c_str(), test_string);
}

}  // namespace
}  // namespace score::memory::shared
