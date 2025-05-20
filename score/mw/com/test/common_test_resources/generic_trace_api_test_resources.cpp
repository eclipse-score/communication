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

#include "score/mw/com/test/common_test_resources/generic_trace_api_test_resources.h"

#include "score/memory/shared/shared_memory_factory.h"
#include "score/os/fcntl.h"
#include "score/os/mman.h"
#include "score/os/stat.h"
#include "score/os/unistd.h"

#include <variant>

namespace bmw
{
    namespace mw
    {
        namespace com
        {
            namespace test
            {

                namespace
                {

                    using testing::_;
                    using testing::An;
                    using testing::Invoke;
                    using testing::Matcher;
                    using testing::Return;
                    using namespace bmw::analysis::tracing;

                    constexpr auto readWriteAccessForUser = os::Stat::Mode::kReadUser | os::Stat::Mode::kWriteUser;
                    constexpr auto readAccessForEveryBody =
                        readWriteAccessForUser | os::Stat::Mode::kReadGroup | os::Stat::Mode::kReadOthers;
                    constexpr auto readWriteAccessForEveryBody =
                        readAccessForEveryBody | os::Stat::Mode::kWriteGroup | os::Stat::Mode::kWriteOthers;

                } // namespace

                void SetupGenericTraceApiMocking(GenericTraceApiMockContext &context) noexcept
                {
                    const TraceClientId trace_client_id{42};
                    ShmObjectHandle shm_object_handle{1};

                    ON_CALL(context.generic_trace_api_mock, RegisterClient(_, _)).WillByDefault(Return(trace_client_id));
                    ON_CALL(context.generic_trace_api_mock, RegisterShmObject(trace_client_id, An<const std::string &>()))
                        .WillByDefault(Return(RegisterSharedMemoryObjectResult{shm_object_handle}));
                    ON_CALL(context.generic_trace_api_mock, UnregisterShmObject(trace_client_id, _))
                        .WillByDefault(Return(ResultBlank{}));
                    ON_CALL(context.generic_trace_api_mock, RegisterTraceDoneCB(trace_client_id, _))
                        .WillByDefault(Invoke([&context](TraceClientId, TraceDoneCallBackType callback)
                                              {
            context.stored_trace_done_cb = std::move(callback);
            return bmw::Result<Blank>{}; }));
                    ON_CALL(context.generic_trace_api_mock, Trace(trace_client_id, _, An<ShmDataChunkList &>(), _))
                        .WillByDefault(Invoke(
                            [&context](TraceClientId, const MetaInfoVariants::type &, ShmDataChunkList &, TraceContextId context_id)
                            {
                                context.last_trace_context_id = context_id;
                                return bmw::Result<Blank>{};
                            }));
                    ON_CALL(context.generic_trace_api_mock, Trace(trace_client_id, _, _)).WillByDefault(Return(bmw::Result<Blank>{}));

                    bmw::memory::shared::SharedMemoryFactory::SetTypedMemoryProvider(context.typed_memory_mock);
                    // Our mock-function for AllocateNamedTypedMemory does the same thing the normal/not-typed-mem allocation does.
                    // So we don't depend on any fancy typed memory, but since AllocateNamedTypedMemory returns a valid shm-object
                    // this is seen as "located in typed memory" and therefore, the skeleton will accept it for doing IPC-Tracing
                    // with it!
                    ON_CALL(*(context.typed_memory_mock), AllocateNamedTypedMemory(_, _, _))
                        .WillByDefault(Invoke([](std::size_t virtual_address_space_to_reserve,
                                                 std::string path,
                                                 const bmw::memory::shared::permission::UserPermissions &permissions)
                                              {
            auto flags = os::Fcntl::Open::kReadWrite | os::Fcntl::Open::kCreate | os::Fcntl::Open::kExclusive;
            os::Stat::Mode mode{};
            if (std::holds_alternative<bmw::memory::shared::permission::WorldWritable>(permissions))
            {
                mode = readWriteAccessForEveryBody;
            }
            else
            {
                if (std::holds_alternative<bmw::memory::shared::permission::WorldReadable>(permissions))
                {
                    mode = readAccessForEveryBody;
                }
                else
                {
                    mode = readWriteAccessForUser;
                }
            }
            const auto result = ::bmw::os::Mman::instance().shm_open(path.data(), flags, mode);
            if (!result.has_value())
            {
                std::cerr << "Provider: Error creating (fake typed-mem) shared-mem-object with errno" << result.error()
                          << std::endl;
                amp::expected_blank<bmw::os::Error> blank{amp::make_unexpected(result.error())};
                return blank;
            }
            const auto truncation_result = ::bmw::os::Unistd::instance().ftruncate(
                result.value(), static_cast<off_t>(virtual_address_space_to_reserve));
            if (!truncation_result.has_value())
            {
                std::cerr << "Provider: Error ftruncating shared-mem-object with errno" << result.error() << std::endl;
                amp::expected_blank<bmw::os::Error> blank{amp::make_unexpected(result.error())};
                return blank;
            }
            return amp::expected_blank<bmw::os::Error>{}; }));
                }

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw
