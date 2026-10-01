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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_SECURECONNECTOR
#define LG_SOMEIP_OSABSTRACTION_SOCKET_SECURECONNECTOR

#include <cstdint>

#if defined(ENABLE_TLS)
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/socket.h>
#endif // ENABLE_TLS

namespace lgsomeip {
namespace osabstraction {

struct SecureConfig {
    bool is_secure_connection = false;
    bool is_reliable = false;
    bool is_server = false;

    SecureConfig(bool secure_connection = false, bool reliable = false, bool server = false)
        : is_secure_connection(secure_connection), is_reliable(reliable), is_server(server) {}
};

#if defined(ENABLE_TLS)

class Address;

// OpenSSL-backed TLS (TCP) / DTLS (UDP) connector. The OpenSSL session type is
// hidden behind a private forward declaration so OpenSSL headers stay out of the
// socket layer.
class SecureConnector {
    struct TlsSession;
    const std::string tcp_connector_ = "TCP";

public:
    explicit SecureConnector(const SecureConfig& config);
    ~SecureConnector();

    void start_tls(const int& file_descriptor);
    void start_dtls(const int& file_descriptor, std::shared_ptr<Address> peer_address);

    std::size_t send(const void* buffer, std::size_t size, std::string address = "", std::uint16_t port = 0);
    std::size_t receive(char* buffer, std::size_t size, std::string address = "", std::uint16_t port = 0);

    bool is_new_dtls_connection(std::string address, std::uint16_t port) const;
    void handle_disconnect(std::string address, std::uint16_t port);

    // Peeks the datagram socket to read the next sender's address without
    // consuming the datagram. Returns true when data is available.
    static bool check_dtls_data_available(int file_descriptor, struct sockaddr* peer_address);

private:
    void init_tls();

    std::map<std::string, std::unique_ptr<TlsSession>> tls_connections_;
    mutable std::mutex tls_map_mutex_;
    bool is_reliable_;
    bool is_server_;
    int socket_fd_;
};

#endif // ENABLE_TLS

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_SECURECONNECTOR
