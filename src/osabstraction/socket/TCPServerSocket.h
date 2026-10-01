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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_TCPSERVERSOCKET
#define LG_SOMEIP_OSABSTRACTION_SOCKET_TCPSERVERSOCKET

#include <netdb.h>
#include <netinet/tcp.h> // TCP_NODELAY
#include <cstdlib>
#include "TCPSocket.h"

namespace lgsomeip {
namespace osabstraction {

class TCPServerSocket : public TCPSocket {
public:
    TCPServerSocket(std::shared_ptr<Address> address, const SecureConfig& secure_config = SecureConfig());
    ~TCPServerSocket() {}

    int listen();
    std::shared_ptr<Socket> accept();
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_TCPSERVERSOCKET
