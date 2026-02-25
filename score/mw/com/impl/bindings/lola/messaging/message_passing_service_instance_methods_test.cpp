#include "score/mw/com/impl/bindings/lola/messaging/client_quality_type.h"
#include "score/mw/com/impl/bindings/lola/messaging/i_message_passing_service.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance.h"
#include "score/mw/com/impl/bindings/lola/methods/method_error.h"
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"
#include "score/mw/com/impl/bindings/lola/methods/skeleton_instance_identifier.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"
#include "score/mw/com/impl/configuration/lola_service_id.h"
#include "score/mw/com/impl/configuration/quality_type.h"
#include "score/mw/com/impl/error_serializer.h"

#include "score/concurrency/executor_mock.h"
#include "score/message_passing/mock/client_connection_mock.h"
#include "score/message_passing/mock/client_factory_mock.h"
#include "score/message_passing/mock/server_connection_mock.h"
#include "score/message_passing/mock/server_factory_mock.h"
#include "score/message_passing/mock/server_mock.h"
#include "score/message_passing/server_types.h"
#include "score/os/mocklib/unistdmock.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/assert_support.hpp>

#include <gtest/gtest.h>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;
using namespace score::message_passing;

enum class MessageWithReplyType : std::uint8_t
{
    kSubscribeServiceMethod = 1,
    kCallMethod,
};

struct SubscribeServiceMethodUnserializedPayload
{
    SkeletonInstanceIdentifier skeleton_instance_identifier;
    ProxyInstanceIdentifier proxy_instance_identifier;
};

struct MethodCallUnserializedPayload
{
    ProxyMethodInstanceIdentifier proxy_method_instance_identifier;
    std::size_t queue_position;
};

using MethodUnserializedReply = score::ResultBlank;
using MethodReplyPayload = ErrorSerializer<MethodErrc>::SerializedErrorType;

constexpr pid_t kLocalPid{1};
constexpr uid_t kLocalUid{3};
constexpr gid_t kLocalGid{4};

constexpr pid_t kRemotePid{10};
constexpr uid_t kRemoteUid{30};
constexpr gid_t kRemoteGid{40};

const ProxyInstanceIdentifier kProxyInstanceIdentifier{kLocalUid, 1U};

const ProxyMethodInstanceIdentifier kProxyMethodInstanceIdentifier{kProxyInstanceIdentifier, LolaMethodId{35U}};
const ProxyMethodInstanceIdentifier kProxyMethodInstanceIdentifier2{kProxyInstanceIdentifier, LolaMethodId{36U}};

const SkeletonInstanceIdentifier kSkeletonInstanceIdentifier{LolaServiceId{12U},
                                                             LolaServiceInstanceId::InstanceId{22U}};
const SkeletonInstanceIdentifier kSkeletonInstanceIdentifier2{LolaServiceId{13U},
                                                              LolaServiceInstanceId::InstanceId{23U}};

constexpr std::size_t kQueuePosition{1U};

MATCHER_P(ContainsError, error_code, "contains a specific error")
{
    return !arg.has_value() && arg.error() == error_code;
}

class MessagePassingServiceInstanceMethodsFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(*server_mock_, StartListening(_, _, _, _))
            .WillByDefault(WithArg<3>(
                Invoke([this](MessageCallback message_received_with_reply_cb) -> score::cpp::expected_blank<score::os::Error> {
                    received_send_message_with_reply_callback_ = std::move(message_received_with_reply_cb);
                    return {};
                })));

        ON_CALL(server_factory_mock_, Create(_, _)).WillByDefault(Return(ByMove(std::move(server_mock_))));

        auto client_connection_mock_facade = score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMockFacade>>(
            score::cpp::pmr::new_delete_resource(), client_connection_mock_);
        ON_CALL(client_factory_mock_, Create(_, _))
            .WillByDefault(Return(ByMove(std::move(client_connection_mock_facade))));

        ON_CALL(client_connection_mock_, SendWaitReply(_, _))
            .WillByDefault(Return(CreateSerializedMethodReply(score::cpp::blank{}, method_reply_buffer_)));
        ON_CALL(client_connection_mock_, GetState()).WillByDefault(testing::Return(IClientConnection::State::kReady));

        ON_CALL(*unistd_mock_, getpid()).WillByDefault(Return(kLocalPid));
        ON_CALL(*unistd_mock_, getuid()).WillByDefault(Return(kLocalUid));

        ON_CALL(executor_mock_, Enqueue(testing::_)).WillByDefault([this](auto&& task) {
            executor_task_ = std::forward<decltype(task)>(task);
        });

        ON_CALL(mock_subscribe_method_handler_, Call(_, _, _)).WillByDefault(Return(score::cpp::blank{}));
    }

    MessagePassingServiceInstanceMethodsFixture& GivenAMessagePassingServiceInstance(
        ClientQualityType client_quality_type = ClientQualityType::kASIL_QM)
    {
        unit_ = std::make_unique<MessagePassingServiceInstance>(
            client_quality_type, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_);
        return *this;
    }

    MessagePassingServiceInstanceMethodsFixture& WithAClientInTheSameProcess()
    {
        client_identity_ = std::make_unique<ClientIdentity>();
        client_identity_->pid = kLocalPid;
        client_identity_->uid = kLocalUid;
        client_identity_->gid = kLocalGid;
        ON_CALL(server_connection_mock_, GetClientIdentity()).WillByDefault(ReturnRef(*client_identity_));
        return *this;
    }

    MessagePassingServiceInstanceMethodsFixture& WithAClientInDifferentProcess()
    {
        client_identity_ = std::make_unique<ClientIdentity>();
        client_identity_->pid = kRemotePid;
        client_identity_->uid = kRemoteUid;
        client_identity_->gid = kRemoteGid;
        ON_CALL(server_connection_mock_, GetClientIdentity()).WillByDefault(ReturnRef(*client_identity_));
        return *this;
    }

    MessagePassingServiceInstanceMethodsFixture& WithARegisteredSubscribeMethodHandler(
        SkeletonInstanceIdentifier skeleton_instance_identifier,
        IMessagePassingService::AllowedConsumerUids allowed_consumer_uids)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(unit_ != nullptr);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(client_identity_ != nullptr);
        IMessagePassingService::ServiceMethodSubscribedHandler scoped_subscribe_method_handler{
            subscribe_method_handler_scope_, mock_subscribe_method_handler_.AsStdFunction()};
        auto result = unit_->RegisterOnServiceMethodSubscribedHandler(
            skeleton_instance_identifier, scoped_subscribe_method_handler, allowed_consumer_uids);
        EXPECT_TRUE(result.has_value());
        return *this;
    }

    MessagePassingServiceInstanceMethodsFixture& WithARegisteredMethodCallHandler(
        ProxyMethodInstanceIdentifier proxy_method_instance_identifier,
        uid_t allowed_consumer_uid)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(unit_ != nullptr);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(client_identity_ != nullptr);
        IMessagePassingService::MethodCallHandler scoped_method_call_handler{method_call_handler_scope_,
                                                                             mock_method_call_handler_.AsStdFunction()};
        auto result = unit_->RegisterMethodCallHandler(
            proxy_method_instance_identifier, scoped_method_call_handler, allowed_consumer_uid);
        EXPECT_TRUE(result.has_value());
        return *this;
    }

    template <typename UnserializedPayload>
    UnserializedPayload DeserializeMethodMessage(score::cpp::span<const std::uint8_t> message,
                                                 MessageWithReplyType message_type)
    {
        EXPECT_EQ(message.size(), (sizeof(UnserializedPayload) + 1U));
        EXPECT_EQ(static_cast<MessageWithReplyType>(message[0]), message_type);

        UnserializedPayload actual_unserialized_payload{};
        std::memcpy(&actual_unserialized_payload, message.data() + 1U, sizeof(UnserializedPayload));
        return actual_unserialized_payload;
    }

    MethodUnserializedReply DeserializeMethodReplyMessage(score::cpp::span<const std::uint8_t> message)
    {
        EXPECT_EQ(message.size(), (sizeof(MethodReplyPayload)));

        MethodReplyPayload actual_unserialized_payload{};
        std::memcpy(&actual_unserialized_payload, message.data(), sizeof(MethodReplyPayload));

        return ErrorSerializer<MethodErrc>::Deserialize(actual_unserialized_payload);
    }

    template <typename T>
    score::cpp::span<const uint8_t> CreateSerializedMethodMessage(const T& el_id, MessageWithReplyType message_type)
    {
        static std::vector<uint8_t> buffer{};
        buffer.assign(sizeof(T) + 1, 0);
        buffer[0] = score::cpp::to_underlying(message_type);
        memcpy(buffer.data() + 1, &el_id, sizeof(T));
        return {buffer.data(), buffer.size()};
    }

    score::cpp::span<const std::uint8_t> CreateSerializedMethodReply(
        score::ResultBlank method_reply,
        std::array<std::uint8_t, sizeof(MethodReplyPayload)>& message_reply_buffer)
    {
        const MethodReplyPayload serialized_com_errc =
            method_reply.has_value()
                ? ErrorSerializer<MethodErrc>::SerializeSuccess()
                : ErrorSerializer<MethodErrc>::SerializeError(static_cast<MethodErrc>(*method_reply.error()));
        score::cpp::ignore = std::memcpy(&message_reply_buffer[0], &serialized_com_errc, sizeof(MethodReplyPayload));
        return {message_reply_buffer.data(), message_reply_buffer.size()};
    }

    score::cpp::span<const uint8_t> CreateValidCallMethodMessage()
    {
        MethodCallUnserializedPayload payload{kProxyMethodInstanceIdentifier, kQueuePosition};
        return CreateSerializedMethodMessage(payload, MessageWithReplyType::kCallMethod);
    }

    score::cpp::span<const uint8_t> CreateValidSubscribeMethodMessage()
    {
        const SubscribeServiceMethodUnserializedPayload payload{kSkeletonInstanceIdentifier, kProxyInstanceIdentifier};
        return CreateSerializedMethodMessage(payload, MessageWithReplyType::kSubscribeServiceMethod);
    }

    NiceMock<ClientFactoryMock> client_factory_mock_{};
    NiceMock<ServerFactoryMock> server_factory_mock_{};

    score::cpp::pmr::unique_ptr<testing::NiceMock<ServerMock>> server_mock_{
        score::cpp::pmr::make_unique<testing::NiceMock<ServerMock>>(score::cpp::pmr::get_default_resource())};

    concurrency::testing::ExecutorMock executor_mock_{};
    score::cpp::pmr::unique_ptr<score::concurrency::Task> executor_task_{};

    AsilSpecificCfg asil_cfg_{};

    NiceMock<ClientConnectionMock> client_connection_mock_{};
    NiceMock<ServerConnectionMock> server_connection_mock_{};

    MessageCallback received_send_message_with_reply_callback_{};

    os::MockGuard<testing::NiceMock<os::UnistdMock>> unistd_mock_{};

    ElementFqId event_id_;

    safecpp::Scope<> method_call_handler_scope_{};
    safecpp::Scope<> subscribe_method_handler_scope_{};

    ::testing::MockFunction<void(std::size_t)> mock_method_call_handler_{};
    ::testing::MockFunction<score::ResultBlank(ProxyInstanceIdentifier, uid_t, pid_t)> mock_subscribe_method_handler_{};

    // Since an SendWaitReply returns an score::cpp::span to a message (which is essentially a pointer to a message), we need a
    // buffer to store the message.
    std::array<std::uint8_t, sizeof(MethodReplyPayload)> method_reply_buffer_{};

    std::unique_ptr<ClientIdentity> client_identity_{nullptr};
    std::unique_ptr<MessagePassingServiceInstance> unit_{nullptr};
};

using MessagePassingServiceInstanceLocalCallMethodTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceLocalCallMethodTest, CallingWithSelfPidCallsMethodHandlerLocally)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that the registered method call handler will be called with the provided queue position
    EXPECT_CALL(mock_method_call_handler_, Call(kQueuePosition));

    // and expecting that a CallMethod message will NOT be sent (which would happen when the client is in another
    // process)
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _)).Times(0);

    // When calling CallMethod with target_node_id equal to the PID of the current process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kLocalPid);

    // Then the result is valid
    ASSERT_TRUE(call_result.has_value());
}

TEST_F(MessagePassingServiceInstanceLocalCallMethodTest, CallingWillFailIfRegisteredUidDoesNotMatchProcessUid)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // Given that the method call handler was registered with an allowed uid that does not match the uid of the local
    // process containing the MessagePassingServiceInstance
    const uid_t invalid_uid = kLocalUid + 20;
    WithARegisteredMethodCallHandler(kProxyMethodInstanceIdentifier, invalid_uid);

    // When calling CallMethod with target_node_id equal to the PID of the current process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceLocalCallMethodTest, CallingWithProxyIdentifierThatWasNeverRegisteredReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier2, client_identity_->uid);

    // When calling CallMethod with a ProxyMethodInstanceIdentifier for which no method call handler has been registered
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceLocalCallMethodTest, CallingAfterMethodHandlerScopeHasExpiredReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // and given that the method call handler scope has expired
    method_call_handler_scope_.Expire();

    // Expecting that the registered method call handler will not be called
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);

    // When calling CallMethod
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

using MessagePassingServiceInstanceRemoteCallMethodTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceRemoteCallMethodTest, CallingWithOtherProcessPidSendsMethodCallMessage)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that a CallMethod message will be sent containing the provided ProxyMethodInstanceIdentifier and queue
    // position
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _)).WillOnce(WithArg<0>(Invoke([this](auto message) {
        const auto actual_payload =
            DeserializeMethodMessage<MethodCallUnserializedPayload>(message, MessageWithReplyType::kCallMethod);
        EXPECT_EQ(actual_payload.queue_position, kQueuePosition);
        EXPECT_EQ(actual_payload.proxy_method_instance_identifier, kProxyMethodInstanceIdentifier);

        return CreateSerializedMethodReply(score::cpp::blank{}, method_reply_buffer_);
    })));

    // and expecting that the registered method call handler will NOT be called (which would happen when the client is
    // in the same process)
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);

    // When calling CallMethod with target_node_id equal to the PID of a different process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kRemotePid);

    // Then the result is valid
    ASSERT_TRUE(call_result.has_value());
}

TEST_F(MessagePassingServiceInstanceRemoteCallMethodTest, CallingGetsClientWithProvidedPid)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that the MessagePassingClient corresponding to the provided PID will be retrieved
    EXPECT_CALL(client_factory_mock_, Create(_, _)).WillOnce(WithArg<0>(Invoke([this](auto protocol_config) {
        const auto expected_identifier = std::string{"LoLa_2_"} + std::to_string(kRemotePid) + "_QM";
        EXPECT_EQ(protocol_config.identifier, expected_identifier);

        auto client_connection_mock_facade = score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMockFacade>>(
            score::cpp::pmr::new_delete_resource(), client_connection_mock_);
        return client_connection_mock_facade;
    })));

    // When calling CallMethod with target_node_id equal to the PID of a different process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kRemotePid);

    // Then the result is valid
    ASSERT_TRUE(call_result.has_value());
}

TEST_F(MessagePassingServiceInstanceRemoteCallMethodTest, ReturnsErrorWhenSendWaitReplyReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that SendWaitReply will be called which returns an error
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno())));

    // When calling CallMethod with target_node_id equal to the PID of a different process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kRemotePid);

    // Then an error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceRemoteCallMethodTest, ReturnsErrorWhenReplyPayloadHasUnexpectedSize)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that SendWaitReply will be called which returns a payload with an unexpected size
    std::vector<std::uint8_t> payload_with_unexpected_size(sizeof(MethodReplyPayload) + 2U);
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _))
        .WillOnce(
            Return(score::cpp::span<std::uint8_t>{payload_with_unexpected_size.data(), payload_with_unexpected_size.size()}));

    // When calling CallMethod with target_node_id equal to the PID of a different process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kRemotePid);

    // Then an error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceRemoteCallMethodTest, ReturnsErrorWhenReplyReportedError)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that SendWaitReply will be called which returns a message payload which reports an error
    const auto error_code{ComErrc::kGrantEnforcementError};
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _))
        .WillOnce(Return(CreateSerializedMethodReply(MakeUnexpected(error_code), method_reply_buffer_)));

    // When calling CallMethod with target_node_id equal to the PID of a different process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kRemotePid);

    // Then the reported error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

using MessagePassingServiceInstanceLocalSubscribeMethodTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceLocalSubscribeMethodTest, CallingWithSelfPidCallsMethodHandlerLocally)
{
    const auto client_quality_type = ClientQualityType::kASIL_QM;
    GivenAMessagePassingServiceInstance(client_quality_type)
        .WithAClientInTheSameProcess()
        .WithARegisteredSubscribeMethodHandler(kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that the registered method subscribe handler will be called with the provided,
    // ProxyInstanceIdentifier and the proxy PID / UID derived from the
    // MessagePassingServiceInstance itself.
    EXPECT_CALL(mock_subscribe_method_handler_, Call(kProxyInstanceIdentifier, kLocalUid, kLocalPid));

    // and expecting that a SubscribeServiceMethod message will NOT be sent (which would happen when the client is in
    // another process)
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _)).Times(0);

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of the current process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kLocalPid);

    // Then the result is valid
    ASSERT_TRUE(call_result.has_value());
}

TEST_F(MessagePassingServiceInstanceLocalSubscribeMethodTest, CallingWillFailIfRegisteredUidDoesNotMatchProcessUid)
{
    const auto client_quality_type = ClientQualityType::kASIL_QM;
    GivenAMessagePassingServiceInstance(client_quality_type).WithAClientInTheSameProcess();

    // Given that the method call handler was registered with a set of allowed uids that does not contain the uid of the
    // local process containing the MessagePassingServiceInstance
    const uid_t invalid_uid = kLocalUid + 20;
    WithARegisteredSubscribeMethodHandler(kSkeletonInstanceIdentifier, {{invalid_uid}});

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of the current process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceLocalSubscribeMethodTest, CallingWillSucceedIfRegisteredWithEmptyUidSet)
{
    const auto client_quality_type = ClientQualityType::kASIL_QM;
    GivenAMessagePassingServiceInstance(client_quality_type).WithAClientInTheSameProcess();

    // Given that the method call handler was registered with a set of allowed uids that does not contain the uid of the
    // local process containing the MessagePassingServiceInstance
    WithARegisteredSubscribeMethodHandler(kSkeletonInstanceIdentifier, IMessagePassingService::AllowedConsumerUids{});

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of the current process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kLocalPid);

    // Then the result is valid
    ASSERT_TRUE(call_result.has_value());
}

TEST_F(MessagePassingServiceInstanceLocalSubscribeMethodTest,
       CallingWithSkeletonIdentifierThatWasNeverRegisteredReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier2, {{client_identity_->uid}});

    // When calling SubscribeServiceMethod with a SkeletonMethodInstanceIdentifier for which no subscribe method handler
    // has been registered
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceLocalSubscribeMethodTest,
       CallingAfterSubscribeMethodHandlerScopeHasExpiredReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // and given that the subscribe method handler scope has expired
    subscribe_method_handler_scope_.Expire();

    // Expecting that the registered subscribe method handler will not be called
    EXPECT_CALL(mock_subscribe_method_handler_, Call(_, _, _)).Times(0);

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of the current process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceLocalSubscribeMethodTest, ReturnsErrorWhenReplyReportedError)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that the registered method subscribe handler will be called which returns an error
    const auto error_code = ComErrc::kCallQueueFull;
    EXPECT_CALL(mock_subscribe_method_handler_, Call(kProxyInstanceIdentifier, kLocalUid, kLocalPid))
        .WillOnce(Return(MakeUnexpected(error_code)));

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of the current process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kLocalPid);

    // Then the result contains the error returned by the handler
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

using MessagePassingServiceInstanceRemoteSubscribeMethodTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceRemoteSubscribeMethodTest, CallingWithOtherProcessPidSendsMethodCallMessage)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that a CallMethod message will be sent containing the provided ProxyInstanceIdentifier and
    // SkeletonInstanceIdentifier
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _)).WillOnce(WithArg<0>(Invoke([this](auto message) {
        const auto actual_payload = DeserializeMethodMessage<SubscribeServiceMethodUnserializedPayload>(
            message, MessageWithReplyType::kSubscribeServiceMethod);
        EXPECT_EQ(actual_payload.skeleton_instance_identifier, kSkeletonInstanceIdentifier);
        EXPECT_EQ(actual_payload.proxy_instance_identifier, kProxyInstanceIdentifier);

        return CreateSerializedMethodReply(score::cpp::blank{}, method_reply_buffer_);
    })));

    // and expecting that the registered method subscribe handler will NOT be called (which would happen when the client
    // is in the same process)
    EXPECT_CALL(mock_subscribe_method_handler_, Call(_, _, _)).Times(0);

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of a different process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kRemotePid);

    // Then the result is valid
    ASSERT_TRUE(call_result.has_value());
}

TEST_F(MessagePassingServiceInstanceRemoteSubscribeMethodTest, CallingGetsClientWithProvidedPid)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that the MessagePassingClient corresponding to the provided PID will be retrieved
    EXPECT_CALL(client_factory_mock_, Create(_, _)).WillOnce(WithArg<0>(Invoke([this](auto protocol_config) {
        const auto expected_identifier = std::string{"LoLa_2_"} + std::to_string(kRemotePid) + "_QM";
        EXPECT_EQ(protocol_config.identifier, expected_identifier);

        auto client_connection_mock_facade = score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMockFacade>>(
            score::cpp::pmr::new_delete_resource(), client_connection_mock_);
        return client_connection_mock_facade;
    })));

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of a different process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kRemotePid);

    // Then the result is valid
    ASSERT_TRUE(call_result.has_value());
}

TEST_F(MessagePassingServiceInstanceRemoteSubscribeMethodTest, ReturnsErrorWhenSendWaitReplyReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that a CallMethod message will be sent which returns an error
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _))
        .WillOnce(Return(score::cpp::make_unexpected(score::os::Error::createFromErrno())));

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of a different process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kRemotePid);

    // Then an error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceRemoteSubscribeMethodTest, ReturnsErrorWhenReplyPayloadHasUnexpectedSize)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that a CallMethod message will be sent which returns a payload with an unexpected size
    std::vector<std::uint8_t> payload_with_unexpected_size(sizeof(MethodReplyPayload) + 2U);
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _))
        .WillOnce(
            Return(score::cpp::span<std::uint8_t>{payload_with_unexpected_size.data(), payload_with_unexpected_size.size()}));

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of a different process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kRemotePid);

    // Then an error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceRemoteSubscribeMethodTest, ReturnsErrorWhenReplyReportedError)
{
    GivenAMessagePassingServiceInstance().WithAClientInDifferentProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that a CallMethod message will be sent which returns a message payload which reports an error
    const auto error_code{ComErrc::kGrantEnforcementError};
    EXPECT_CALL(client_connection_mock_, SendWaitReply(_, _))
        .WillOnce(Return(CreateSerializedMethodReply(MakeUnexpected(error_code), method_reply_buffer_)));

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of a different process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kRemotePid);

    // Then the reported error is returned
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

using MessagePassingServiceInstanceRegisterMethodCallHandlerTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceRegisterMethodCallHandlerTest, ReregisteringHandlerOverwritesStoredHandler)
{
    ::testing::MockFunction<void(std::size_t)> mock_method_call_handler_2{};
    safecpp::Scope<> method_call_handler_scope_2{};
    IMessagePassingService::MethodCallHandler scoped_method_call_handler_2{method_call_handler_scope_2,
                                                                           mock_method_call_handler_2.AsStdFunction()};

    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that only the newly registered method call handler will be called
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);
    EXPECT_CALL(mock_method_call_handler_2, Call(_));

    // When registering a new method call handler
    auto result = unit_->RegisterMethodCallHandler(
        kProxyMethodInstanceIdentifier, scoped_method_call_handler_2, client_identity_->uid);
    EXPECT_TRUE(result.has_value());

    // Then when calling the method
    score::cpp::ignore = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kLocalPid);
}

using MessagePassingServiceInstanceRegisterSubscribeHandlerTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceRegisterSubscribeHandlerTest, ReregisteringHandlerReturnsError)
{
    ::testing::MockFunction<score::ResultBlank(ProxyInstanceIdentifier, uid_t, pid_t)> mock_subscribe_method_handler_2{};
    safecpp::Scope<> subscribe_method_handler_scope_2{};
    IMessagePassingService::ServiceMethodSubscribedHandler scoped_subscribe_method_handler_2{
        subscribe_method_handler_scope_2, mock_subscribe_method_handler_2.AsStdFunction()};

    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // When registering a new on method subscribed handler
    auto result = unit_->RegisterOnServiceMethodSubscribedHandler(
        kSkeletonInstanceIdentifier, scoped_subscribe_method_handler_2, {{client_identity_->uid}});

    // Then an error is returned
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

using MessagePassingServiceInstanceHandleMessageWithReplyTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceHandleMessageWithReplyTest, ReturnsErrorWhenEmptyMessageReceived)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // When a MessageWithReply message is received which is empty
    std::vector<std::uint8_t> empty_message(0U);
    const auto result = received_send_message_with_reply_callback_(
        server_connection_mock_, score::cpp::span<std::uint8_t>{empty_message.data(), empty_message.size()});

    // Then an error is returned since the error is unrecoverable
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), os::Error::Code::kUnexpected);
}

TEST_F(MessagePassingServiceInstanceHandleMessageWithReplyTest, RepliesWithErrorWhenEmptyMessageReceived)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // Expecting that a reply will be sent containing an unexpected message size error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kUnexpectedMessageSize));
            return {};
        }));

    // When a MessageWithReply message is received which is empty
    std::vector<std::uint8_t> empty_message(0U);
    const auto result = received_send_message_with_reply_callback_(
        server_connection_mock_, score::cpp::span<std::uint8_t>{empty_message.data(), empty_message.size()});
}

TEST_F(MessagePassingServiceInstanceHandleMessageWithReplyTest, ReturnsErrorWhenUnexpectedMessageReceived)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // When a MessageWithReply message is received of unexpected type
    std::vector<std::uint8_t> payload_with_unexpected_type(sizeof(MethodCallUnserializedPayload) + 2U);
    payload_with_unexpected_type[0] = 20U;
    const auto result = received_send_message_with_reply_callback_(
        server_connection_mock_,
        score::cpp::span<std::uint8_t>{payload_with_unexpected_type.data(), payload_with_unexpected_type.size()});

    // Then an error is returned since the error is unrecoverable
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), os::Error::Code::kUnexpected);
}

TEST_F(MessagePassingServiceInstanceHandleMessageWithReplyTest, RepliesWithErrorWhenUnexpectedMessageReceived)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // Expecting that a reply will be sent containing an unexpected message error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kUnexpectedMessage));
            return {};
        }));

    // When a MessageWithReply message is received of unexpected type
    std::vector<std::uint8_t> payload_with_unexpected_type(sizeof(MethodCallUnserializedPayload) + 2U);
    payload_with_unexpected_type[0] = 20U;
    const auto result = received_send_message_with_reply_callback_(
        server_connection_mock_,
        score::cpp::span<std::uint8_t>{payload_with_unexpected_type.data(), payload_with_unexpected_type.size()});
}

using MessagePassingServiceInstanceHandleCallMethodMessageTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, ReturnsErrorWhenPayloadHasUnexpectedSize)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that the registered method call handler will not be called
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);

    // When a MessageWithReply message is received of type kCallMethod with the wrong payload size
    std::vector<std::uint8_t> payload_with_unexpected_size(sizeof(MethodCallUnserializedPayload) + 2U);
    payload_with_unexpected_size[0] = static_cast<std::uint8_t>(MessageWithReplyType::kCallMethod);
    const auto result = received_send_message_with_reply_callback_(
        server_connection_mock_,
        score::cpp::span<std::uint8_t>{payload_with_unexpected_size.data(), payload_with_unexpected_size.size()});

    // Then an error is returned since the error is unrecoverable
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), os::Error::Code::kUnexpected);
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, RepliesWithErrorWhenPayloadHasUnexpectedSize)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that a reply will be sent containing an unexpected message size error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kUnexpectedMessageSize));
            return {};
        }));

    // When a MessageWithReply message is received of type kCallMethod with the wrong payload size
    std::vector<std::uint8_t> payload_with_unexpected_size(sizeof(MethodCallUnserializedPayload) + 2U);
    payload_with_unexpected_size[0] = static_cast<std::uint8_t>(MessageWithReplyType::kCallMethod);
    score::cpp::ignore = received_send_message_with_reply_callback_(
        server_connection_mock_,
        score::cpp::span<std::uint8_t>{payload_with_unexpected_size.data(), payload_with_unexpected_size.size()});
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, ReturnsSuccessWhenHandlerNotRegistered)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // Expecting that the registered method call handler will not be called
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);

    // When a valid MessageWithReply message is received of type kCallMethod when no method call handler has been
    // registered
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());

    // Then a valid result is returned since the error is a recoverable error
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, RepliesWithErrorWhenHandlerNotRegistered)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // Expecting that a reply will be sent containing a not subscribed error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kNotSubscribed));
            return {};
        }));

    // When a valid MessageWithReply message is received of type kCallMethod when no method call handler has been
    // registered
    score::cpp::ignore = received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, ReturnsSuccessWhenHandlerScopeAlreadyExpired)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // and given that the method call handler scope has expired (which will trigger a recoverable error)
    method_call_handler_scope_.Expire();

    // Expecting that the registered method call handler will not be called
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);

    // When a valid MessageWithReply message is received of type kCallMethod
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());

    // Then a valid result is returned (the error will be serialized and sent back to the caller, but the callback
    // itself will not return an error)
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, RepliesWithErrorWhenHandlerScopeAlreadyExpired)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // and given that the method call handler scope has expired (which will trigger a recoverable error)
    method_call_handler_scope_.Expire();

    // Expecting that a reply will be sent containing a skeleton already destroyed error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kSkeletonAlreadyDestroyed));
            return {};
        }));

    // When a valid MessageWithReply message is received of type kCallMethod
    score::cpp::ignore = received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest,
       CallsCallMethodHandlerRegisteredWithProvidedProxyIdentifier)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that the registered method call handler will be called with the provided queue position
    EXPECT_CALL(mock_method_call_handler_, Call(kQueuePosition));

    // When a valid MessageWithReply message is received of type kCallMethod
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());

    // Then a valid result is returned
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, RepliesSuccessWhenMethodHandlerCalledSuccessfully)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that a reply will be sent containing success
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_TRUE(reply_result.has_value());
            return {};
        }));

    // When a valid MessageWithReply message is received of type kCallMethod
    score::cpp::ignore = received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, ReturnsSuccessWhenCallerUidDoesNotMatchRegisteredUid)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that GetClientIdentity will be called which returns a uid different to the one registered in
    // RegisterMethodCallHandler
    auto invalid_client_identity = *client_identity_;
    invalid_client_identity.uid += 30;
    EXPECT_CALL(server_connection_mock_, GetClientIdentity()).WillOnce(ReturnRef(invalid_client_identity));

    // Expecting that the registered method call handler will not be called
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);

    // When a valid MessageWithReply message is received of type kCallMethod
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());

    // Then a valid result is returned since the error is recoverable
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleCallMethodMessageTest, RepliesErrorWhenCallerUidDoesNotMatchRegisteredUid)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // Expecting that GetClientIdentity will be called which returns a uid different to the one registered in
    // RegisterMethodCallHandler
    auto invalid_client_identity = *client_identity_;
    invalid_client_identity.uid += 30;
    EXPECT_CALL(server_connection_mock_, GetClientIdentity()).WillOnce(ReturnRef(invalid_client_identity));

    // Expecting that a reply will be sent containing an unknown proxy error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kUnknownProxy));
            return {};
        }));

    // When a valid MessageWithReply message is received of type kCallMethod
    score::cpp::ignore = received_send_message_with_reply_callback_(server_connection_mock_, CreateValidCallMethodMessage());
}

using MessagePassingServiceInstanceHandleSubscribeMethodMessageTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest, ReturnsErrorWhenPayloadHasUnexpectedSize)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // When a MessageWithReply message is received of type kSubscribeServiceMethod with the wrong payload size
    std::vector<std::uint8_t> payload_with_unexpected_size(sizeof(SubscribeServiceMethodUnserializedPayload) + 2U);
    payload_with_unexpected_size[0] = static_cast<std::uint8_t>(MessageWithReplyType::kSubscribeServiceMethod);
    const auto result = received_send_message_with_reply_callback_(
        server_connection_mock_,
        score::cpp::span<std::uint8_t>{payload_with_unexpected_size.data(), payload_with_unexpected_size.size()});

    // Then an error is returned since the error is unrecoverable
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), os::Error::Code::kUnexpected);
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest, RepliesWithErrorWhenPayloadHasUnexpectedSize)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that a reply will be sent containing an unexpected message size error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kUnexpectedMessageSize));
            return {};
        }));

    // When a MessageWithReply message is received of type kSubscribeServiceMethod with the wrong payload size
    std::vector<std::uint8_t> payload_with_unexpected_size(sizeof(SubscribeServiceMethodUnserializedPayload) + 2U);
    payload_with_unexpected_size[0] = static_cast<std::uint8_t>(MessageWithReplyType::kSubscribeServiceMethod);
    score::cpp::ignore = received_send_message_with_reply_callback_(
        server_connection_mock_,
        score::cpp::span<std::uint8_t>{payload_with_unexpected_size.data(), payload_with_unexpected_size.size()});
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest, ReturnsSuccessWhenHandlerNotRegistered)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // Expecting that the registered subscribe method handler will not be called
    EXPECT_CALL(mock_subscribe_method_handler_, Call(_, _, _)).Times(0);

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod when no method call handler has
    // been registered
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());

    // Then a valid result is returned since the error is a recoverable error
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest, RepliesWithErrorWhenHandlerNotRegistered)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // Expecting that a reply will be sent containing a not subscribed error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kNotOffered));
            return {};
        }));

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod when no method call handler has
    // been registered
    score::cpp::ignore =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest, ReturnsSuccessWhenHandlerScopeAlreadyExpired)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // and given that the subscribe method handler scope has expired (which will trigger a recoverable error)
    subscribe_method_handler_scope_.Expire();

    // Expecting that the registered subscribe method handler will not be called
    EXPECT_CALL(mock_subscribe_method_handler_, Call(_, _, _)).Times(0);

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());

    // Then a valid result is returned since the error is a recoverable error
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest, RepliesWithErrorWhenHandlerScopeAlreadyExpired)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // and given that the subscribe method handler scope has expired (which will trigger a recoverable error)
    subscribe_method_handler_scope_.Expire();

    // Expecting that a reply will be sent containing a skeleton already destroyed error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kSkeletonAlreadyDestroyed));
            return {};
        }));

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod
    score::cpp::ignore =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest,
       CallsSubscribeMethodHandlerRegisteredWithProvidedSkeletonIdentifier)
{
    const auto client_quality_type = ClientQualityType::kASIL_QM;
    GivenAMessagePassingServiceInstance(client_quality_type)
        .WithAClientInTheSameProcess()
        .WithARegisteredSubscribeMethodHandler(kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that the registered method subscribe handler will be called with the provided,
    // ProxyInstanceIdentifier and the proxy PID / UID derived from the
    // MessagePassingServiceInstance itself.
    EXPECT_CALL(mock_subscribe_method_handler_, Call(kProxyInstanceIdentifier, kLocalUid, kLocalPid));

    // When a message a MessageWithReply message is received of type kSubscribeServiceMethod
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());

    // Then a valid result is returned
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest,
       RepliesSuccessWhenSubscribeMethodHandlerCalledSuccessfully)
{
    const auto client_quality_type = ClientQualityType::kASIL_QM;
    GivenAMessagePassingServiceInstance(client_quality_type)
        .WithAClientInTheSameProcess()
        .WithARegisteredSubscribeMethodHandler(kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that a reply will be sent containing success
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_TRUE(reply_result.has_value());
            return {};
        }));

    // When a message a MessageWithReply message is received of type kSubscribeServiceMethod
    score::cpp::ignore =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest,
       ReturnsSuccessWhenCallerUidDoesNotMatchRegisteredUid)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that GetClientIdentity will be called which returns a uid different to the one registered in
    // RegisterOnServiceMethodSubscribedHandler
    auto invalid_client_identity = *client_identity_;
    invalid_client_identity.uid += 30;
    EXPECT_CALL(server_connection_mock_, GetClientIdentity()).WillOnce(ReturnRef(invalid_client_identity));

    // Expecting that the registered subscribe method handler will not be called
    EXPECT_CALL(mock_subscribe_method_handler_, Call(_, _, _)).Times(0);

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());

    // Then a valid result is returned since the error is recoverable
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest,
       RepliesErrorWhenCallerUidDoesNotMatchRegisteredUid)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // Expecting that GetClientIdentity will be called which returns a uid different to the one registered in
    // RegisterOnServiceMethodSubscribedHandler
    auto invalid_client_identity = *client_identity_;
    invalid_client_identity.uid += 30;
    EXPECT_CALL(server_connection_mock_, GetClientIdentity()).WillOnce(ReturnRef(invalid_client_identity));

    // Expecting that a reply will be sent containing an unknown proxy error
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_THAT(reply_result, ContainsError(MethodErrc::kUnknownProxy));
            return {};
        }));

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod
    score::cpp::ignore =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest,
       ReturnsSuccessWhenHandlerRegisteredAllowingAllUids)
{
    const IMessagePassingService::AllowedConsumerUids allow_all_consumer_ids{};
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, allow_all_consumer_ids);

    // Expecting that GetClientIdentity will be called which returns a random uid
    auto invalid_client_identity = *client_identity_;
    invalid_client_identity.uid += 30;
    EXPECT_CALL(server_connection_mock_, GetClientIdentity()).WillOnce(ReturnRef(invalid_client_identity));

    // Expecting that the registered subscribe method handler will be called with the uid returned by
    // GetClientIdentity()
    EXPECT_CALL(mock_subscribe_method_handler_, Call(kProxyInstanceIdentifier, invalid_client_identity.uid, kLocalPid));

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod
    const auto result =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());

    // Then a valid result is returned since the error is recoverable
    ASSERT_TRUE(result.has_value());
}

TEST_F(MessagePassingServiceInstanceHandleSubscribeMethodMessageTest,
       RepliesSuccessWhenHandlerRegisteredAllowingAllUids)
{
    const IMessagePassingService::AllowedConsumerUids allow_all_consumer_ids{};
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, allow_all_consumer_ids);

    // Expecting that GetClientIdentity will be called which returns a random uid
    auto invalid_client_identity = *client_identity_;
    invalid_client_identity.uid += 30;
    EXPECT_CALL(server_connection_mock_, GetClientIdentity()).WillOnce(ReturnRef(invalid_client_identity));

    // Expecting that a reply will be sent containing success
    EXPECT_CALL(server_connection_mock_, Reply(_))
        .WillOnce(Invoke([this](auto reply_buffer) -> score::cpp::expected_blank<score::os::Error> {
            const auto reply_result = DeserializeMethodReplyMessage(reply_buffer);
            EXPECT_TRUE(reply_result.has_value());
            return {};
        }));

    // When a valid MessageWithReply message is received of type kSubscribeServiceMethod
    score::cpp::ignore =
        received_send_message_with_reply_callback_(server_connection_mock_, CreateValidSubscribeMethodMessage());
}

using MessagePassingServiceInstanceUnregisterMethodCallHandlerTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceUnregisterMethodCallHandlerTest, CallingHandlerAfterUnregisteringReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredMethodCallHandler(
        kProxyMethodInstanceIdentifier, client_identity_->uid);

    // and given that the handler has been unregistered
    unit_->UnregisterMethodCallHandler(kProxyMethodInstanceIdentifier);

    // Expecting that the registered method call handler will be not called
    EXPECT_CALL(mock_method_call_handler_, Call(_)).Times(0);

    // When calling CallMethod with target_node_id equal to the PID of the current process
    const auto call_result = unit_->CallMethod(kProxyMethodInstanceIdentifier, kQueuePosition, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceUnregisterMethodCallHandlerTest, CallingUnregisterHandlerBeforeRegisterTerminates)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // When calling UnregisterMethodCallHandler before registering a handler
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(unit_->UnregisterMethodCallHandler(kProxyMethodInstanceIdentifier));
}

using MessagePassingServiceInstanceUnregisterSubscribeMethodHandlerTest = MessagePassingServiceInstanceMethodsFixture;
TEST_F(MessagePassingServiceInstanceUnregisterSubscribeMethodHandlerTest, CallingHandlerAfterUnregisteringReturnsError)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess().WithARegisteredSubscribeMethodHandler(
        kSkeletonInstanceIdentifier, {{client_identity_->uid}});

    // and given that the handler has been unregistered
    unit_->UnregisterOnServiceMethodSubscribedHandler(kSkeletonInstanceIdentifier);

    // Expecting that the registered method subscribe handler will not be called
    EXPECT_CALL(mock_subscribe_method_handler_, Call(_, _, _)).Times(0);

    // When calling SubscribeServiceMethod with target_node_id equal to the PID of the current process
    const auto call_result =
        unit_->SubscribeServiceMethod(kSkeletonInstanceIdentifier, kProxyInstanceIdentifier, kLocalPid);

    // Then the result contains an error
    ASSERT_FALSE(call_result.has_value());
    EXPECT_EQ(call_result.error(), ComErrc::kBindingFailure);
}

TEST_F(MessagePassingServiceInstanceUnregisterSubscribeMethodHandlerTest,
       CallingUnregisterHandlerBeforeRegisterTerminates)
{
    GivenAMessagePassingServiceInstance().WithAClientInTheSameProcess();

    // When calling UnregisterOnServiceMethodSubscribedHandler before registering a handler
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(unit_->UnregisterOnServiceMethodSubscribedHandler(kSkeletonInstanceIdentifier));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
