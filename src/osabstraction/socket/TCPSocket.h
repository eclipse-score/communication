/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_TCPSOCKET
#define LG_SOMEIP_OSABSTRACTION_SOCKET_TCPSOCKET

#include "Socket.h"

namespace lgsomeip {
namespace osabstraction {

class TCPSocket : public Socket {
public:
    TCPSocket(std::shared_ptr<Address> source_address, std::shared_ptr<Address> destination_address,
              const SecureConfig& secure_config = SecureConfig());
    TCPSocket(int file_descriptor, std::shared_ptr<Address> destination_address,
              const SecureConfig& secure_config = SecureConfig());
    virtual ~TCPSocket() {}

    int send(int file_descriptor, const void* buffer, std::size_t size) override;
    int receive(char* buffer, std::size_t size) override;
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_TCPSOCKET
