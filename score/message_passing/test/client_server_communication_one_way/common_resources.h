#ifndef SCORE_MESSAGE_PASSING_TEST_CLIENT_SERVER_COMMUNICATION_ONE_WAY_COMMON_RESOURCES_H
#define SCORE_MESSAGE_PASSING_TEST_CLIENT_SERVER_COMMUNICATION_ONE_WAY_COMMON_RESOURCES_H
#include "score/message_passing/service_protocol_config.h"

namespace score::message_passing::test
{

constexpr std::string_view service_identifier{"test_server"};
// we only send and receive one byte for in this tests
constexpr std::uint32_t kMaxSendSize{1U};
constexpr std::uint32_t kMaxReplySize{1U};

const score::message_passing::ServiceProtocolConfig kTestServiceProtocolConfig{service_identifier,
                                                                               kMaxSendSize,
                                                                               kMaxReplySize,
                                                                               0U};

}  // namespace score::message_passing::test

#endif  // SCORE_MESSAGE_PASSING_TEST_CLIENT_SERVER_COMMUNICATION_ONE_WAY_COMMON_RESOURCES_H
