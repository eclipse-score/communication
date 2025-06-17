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
#include "score/mw/com/impl/bindings/lola/messaging/node_identifier_copier.h"

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"

#include <score/assert.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace score::mw::com::impl::lola::test
{

const ElementFqId kDummyElementFqId{2U, 3U, 4U, ServiceElementType::EVENT};
const ElementFqId kElementFqIdNotInMap{222U, 333U, 444U, ServiceElementType::EVENT};
const pid_t kPidNotInMap{10000U};
constexpr auto kMaxBufferSize = std::tuple_size<NodeIdTmpBufferType>::value;

class CopyNodeIdentiersFixture : public ::testing::Test
{
  public:
    CopyNodeIdentiersFixture& WithANodeIdMapContainingPids(const ElementFqId element_fq_id_map_key,
                                                           std::vector<pid_t> pids)
    {
        std::set<pid_t> pid_set{pids.begin(), pids.end()};
        node_id_map_.insert({element_fq_id_map_key, std::move(pid_set)});
        return *this;
    }

    std::uint32_t GetNumberOfPidsInBuffer() const
    {
        std::uint32_t number_of_valid_node_ids{0U};
        for (const auto node_id : node_id_tmp_buffer_)
        {
            if (node_id != 0U)
            {
                number_of_valid_node_ids += 1U;
            }
        }
        return number_of_valid_node_ids;
    }

    bool ArePidsInNodeBuffer(std::vector<pid_t> expected_pids) const
    {
        EXPECT_EQ(expected_pids.size(), GetNumberOfPidsInBuffer());
        if (expected_pids.size() != GetNumberOfPidsInBuffer())
        {
            return false;
        }

        for (std::size_t i = 0; i < expected_pids.size(); ++i)
        {
            const auto actual_pid = node_id_tmp_buffer_[i];
            const auto expected_pid = expected_pids[i];
            EXPECT_EQ(actual_pid, expected_pid);
            if (actual_pid != expected_pid)
            {
                return false;
            }
        }
        return true;
    }

    using MapType = std::unordered_map<ElementFqId, std::set<pid_t>>;
    MapType node_id_map_{};
    NodeIdTmpBufferType node_id_tmp_buffer_{};
    std::shared_mutex map_mutex_{};
};

using CopyNodeIdentiersEmptyMapFixture = CopyNodeIdentiersFixture;
TEST_F(CopyNodeIdentiersEmptyMapFixture, CopyingNodesReturnsNoFutherIdsAvailable)
{
    // Given a map containg no node ids
    const std::vector<pid_t> node_ids{};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying node identifiers
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, kPidNotInMap);

    // Then no further ids are available
    EXPECT_FALSE(further_ids_available);
}

TEST_F(CopyNodeIdentiersEmptyMapFixture, CopyingNodesCopiesNoIds)
{
    // Given a map containg no node ids
    const std::vector<pid_t> node_ids{};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying node identifiers
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, kPidNotInMap);

    // Then no node ids are copied to the buffer
    EXPECT_EQ(num_of_nodeids_copied, 0U);
    EXPECT_EQ(GetNumberOfPidsInBuffer(), 0U);
}

using CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture = CopyNodeIdentiersFixture;
TEST_F(CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture, CopyingNodesReturnsNoFurtherIdsAvailable)
{
    // Given a map containing less node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, node_ids[0]);

    // Then no further ids are available
    EXPECT_FALSE(further_ids_available);
}

TEST_F(CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture, CopyingNodesCopiesAllIdsFromMap)
{
    // Given a map containing less node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, node_ids[0]);

    // Then all node ids are copied to the buffer
    const std::vector<pid_t> expected_copied_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    EXPECT_EQ(num_of_nodeids_copied, expected_copied_ids.size());
    EXPECT_TRUE(ArePidsInNodeBuffer(expected_copied_ids));
}

TEST_F(CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture,
       CopyingNodesStartingFromSecondIdReturnsNoFurtherIdsAvailable)
{
    // Given a map containing less node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map starting from the second pid in the map
    const auto second_pid = node_ids[1];
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, second_pid);

    // Then no further ids are available
    EXPECT_FALSE(further_ids_available);
}

TEST_F(CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture,
       CopyingNodesStartingFromSecondIdCopiesAllIdsExceptFirst)
{
    // Given a map containing less node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map starting from the second pid in the map
    const auto second_pid = node_ids[1];
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, second_pid);

    // Then all node ids starting from the second id are copied to the buffer
    const std::vector<pid_t> expected_copied_ids{2, 3, 4, 5, 6, 7, 8, 9, 10};
    EXPECT_EQ(num_of_nodeids_copied, expected_copied_ids.size());
    EXPECT_TRUE(ArePidsInNodeBuffer(expected_copied_ids));
}

TEST_F(CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture,
       CopyingNodesStartingFromIdNotInMapReturnsNoFurtherIdsAvailable)
{
    // Given a map containing less node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map starting from a pid that is not in the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, kPidNotInMap);

    // Then no further ids are available
    EXPECT_FALSE(further_ids_available);
}

TEST_F(CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture, CopyingNodesStartingFromIdNotInMapCopiesNoIds)
{
    // Given a map containing less node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map starting from a pid that is not in the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, kPidNotInMap);

    // Then no node ids are copied to the buffer
    EXPECT_EQ(num_of_nodeids_copied, 0U);
    EXPECT_EQ(GetNumberOfPidsInBuffer(), 0U);
}

TEST_F(CopyNodeIdentiersMapWithLessNodesThanMaxBufferSizeFixture, CopyingNodesForElementFqIdNotInMapCopiesNoIds)
{
    // Given a map containing less node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers for an ElementFqId that's not in the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kElementFqIdNotInMap, node_id_map_, map_mutex_, node_id_tmp_buffer_, node_ids[0]);

    // Then no node ids are copied to the buffer
    EXPECT_EQ(num_of_nodeids_copied, 0U);
    EXPECT_EQ(GetNumberOfPidsInBuffer(), 0U);
}

using CopyNodeIdentiersMapWithNodesEqualToMaxBufferSizeFixture = CopyNodeIdentiersFixture;
TEST_F(CopyNodeIdentiersMapWithNodesEqualToMaxBufferSizeFixture, CopyingNodesReturnsNoFurtherIdsAvailable)
{
    // Given a map containing the same number of node ids as the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    ASSERT_EQ(node_ids.size(), kMaxBufferSize);
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, node_ids[0]);

    // Then no further ids are available
    EXPECT_FALSE(further_ids_available);
}

TEST_F(CopyNodeIdentiersMapWithNodesEqualToMaxBufferSizeFixture, CopyingNodesCopiesAllIdsFromMap)
{
    // Given a map containing the same number of node ids as the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    ASSERT_EQ(node_ids.size(), kMaxBufferSize);
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, node_ids[0]);

    // Then all the ids are copied to the buffer
    const std::vector<pid_t> expected_copied_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    EXPECT_EQ(num_of_nodeids_copied, kMaxBufferSize);
    EXPECT_TRUE(ArePidsInNodeBuffer(expected_copied_ids));
}

using CopyNodeIdentiersMapWithMoreNodesThanMaxBufferSizeFixture = CopyNodeIdentiersFixture;
TEST_F(CopyNodeIdentiersMapWithMoreNodesThanMaxBufferSizeFixture, CopyingNodesReturnsFurtherIdsAvailable)
{
    // Given a map containing more node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
    ASSERT_GT(node_ids.size(), kMaxBufferSize);
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, node_ids[0]);

    // Then further ids are available
    EXPECT_TRUE(further_ids_available);
}

TEST_F(CopyNodeIdentiersMapWithMoreNodesThanMaxBufferSizeFixture, CopyingNodesCopiesFirstMaxBufferSizeIdsFromMap)
{
    // Given a map containing more node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
    ASSERT_GT(node_ids.size(), kMaxBufferSize);
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, node_ids[0]);

    // Then the first <max buffer size> ids are copied to the buffer
    const std::vector<pid_t> expected_copied_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    EXPECT_EQ(num_of_nodeids_copied, kMaxBufferSize);
    EXPECT_TRUE(ArePidsInNodeBuffer(expected_copied_ids));
}

TEST_F(CopyNodeIdentiersMapWithMoreNodesThanMaxBufferSizeFixture,
       CopyingNodesStartingFromSecondIdCopiesMaxBufferSizeIdsStartingFromSecond)
{
    // Given a map containing more node ids than the max buffer size
    const std::vector<pid_t> node_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
    ASSERT_GT(node_ids.size(), kMaxBufferSize);
    WithANodeIdMapContainingPids(kDummyElementFqId, node_ids);

    // When copying the node identifiers from the map
    const auto second_pid = node_ids[1];
    const auto [num_of_nodeids_copied, further_ids_available] =
        CopyNodeIdentifiers<MapType>(kDummyElementFqId, node_id_map_, map_mutex_, node_id_tmp_buffer_, second_pid);

    // Then the first <max buffer size> ids starting from the second pid are copied to the buffer
    const std::vector<pid_t> expected_copied_ids{2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                                                 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
    EXPECT_EQ(num_of_nodeids_copied, kMaxBufferSize);
    EXPECT_TRUE(ArePidsInNodeBuffer(expected_copied_ids));
}

}  // namespace score::mw::com::impl::lola::test
