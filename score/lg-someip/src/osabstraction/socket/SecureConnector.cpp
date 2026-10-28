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

#include <errno.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "Address.h"
#include "utils/log/logger.h"
#include "SecureConnector.h"

namespace lgsomeip {
namespace osabstraction {

namespace {

// PSK identity hint advertised during the handshake.
#if defined(QNX)
constexpr char kPskIdentityHint[] = "0073";
#else
constexpr char kPskIdentityHint[] = "aaaa";
#endif

// Loads the pre-shared key (hex-encoded) from the SOMEIP_TLS_PSK environment
// variable. Returns the number of key bytes written, or 0 when no valid key is
// provisioned. The key is never hardcoded; provisioning it is the integrator's
// responsibility and the handshake fails closed when it is absent.
std::size_t load_pre_shared_key(unsigned char* output, std::size_t max_length) {
    const char* hex = ::getenv("SOMEIP_TLS_PSK");
    if (hex == nullptr) {
        return 0;
    }

    std::size_t hex_length = std::strlen(hex);
    if (hex_length == 0 || (hex_length % 2) != 0 || (hex_length / 2) > max_length) {
        return 0;
    }

    for (std::size_t i = 0; i < hex_length; i += 2) {
        unsigned int byte = 0;
        if (std::sscanf(hex + i, "%2x", &byte) != 1) {
            return 0;
        }
        output[i / 2] = static_cast<unsigned char>(byte);
    }

    return hex_length / 2;
}

unsigned int psk_client_callback(SSL*, const char*, char* identity, unsigned int max_identity_length,
                                 unsigned char* pre_shared_key, unsigned int max_psk_length) {
    std::snprintf(identity, max_identity_length, "%s", kPskIdentityHint);
    return static_cast<unsigned int>(load_pre_shared_key(pre_shared_key, max_psk_length));
}

unsigned int psk_server_callback(SSL*, const char* /*identity*/, unsigned char* pre_shared_key,
                                 unsigned int max_psk_length) {
    return static_cast<unsigned int>(load_pre_shared_key(pre_shared_key, max_psk_length));
}

} // namespace

// OpenSSL session wrapper. Mirrors the small surface the connector needs so that
// TLS (TCP) and DTLS (UDP) share the same handshake/transfer code paths.
struct SecureConnector::TlsSession {
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    bool is_dtls = false;
    bool is_server = false;

    TlsSession(bool dtls, bool server) : is_dtls(dtls), is_server(server) {
        const SSL_METHOD* method = dtls ? (server ? DTLS_server_method() : DTLS_client_method())
                                        : (server ? TLS_server_method() : TLS_client_method());

        ctx = SSL_CTX_new(method);
        if (ctx == nullptr) {
            return;
        }

        if (is_dtls) {
            SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
            SSL_CTX_set_max_proto_version(ctx, DTLS1_2_VERSION);
        } else {
            SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
            SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION);
        }

        SSL_CTX_set_cipher_list(ctx, "PSK");

        if (is_server) {
            SSL_CTX_use_psk_identity_hint(ctx, kPskIdentityHint);
            SSL_CTX_set_psk_server_callback(ctx, psk_server_callback);
        } else {
            SSL_CTX_set_psk_client_callback(ctx, psk_client_callback);
        }
    }

    ~TlsSession() {
        if (ssl != nullptr) {
            SSL_free(ssl);
        }
        if (ctx != nullptr) {
            SSL_CTX_free(ctx);
        }
    }

    bool create_ssl() {
        if (ctx == nullptr) {
            return false;
        }
        ssl = SSL_new(ctx);
        return ssl != nullptr;
    }

    void set_fd(int file_descriptor) {
        if (ssl == nullptr) {
            return;
        }
        if (is_dtls) {
            BIO* bio = BIO_new_dgram(file_descriptor, BIO_NOCLOSE);
            SSL_set_bio(ssl, bio, bio);
        } else {
            SSL_set_fd(ssl, file_descriptor);
        }
    }

    void set_peer_info(const sockaddr* address) {
        if (ssl == nullptr || address == nullptr) {
            return;
        }
        BIO* bio = SSL_get_rbio(ssl);
        if (bio != nullptr) {
            BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, const_cast<sockaddr*>(address));
        }
    }

    int handshake() {
        if (ssl == nullptr) {
            return -1;
        }
        int ret = is_server ? SSL_accept(ssl) : SSL_connect(ssl);
        return (ret == 1) ? 0 : -1;
    }

    int send(const void* buffer, std::size_t size) {
        if (ssl == nullptr) {
            return -1;
        }
        return SSL_write(ssl, buffer, static_cast<int>(size));
    }

    int recv(void* buffer, std::size_t size) {
        if (ssl == nullptr) {
            return -1;
        }
        return SSL_read(ssl, buffer, static_cast<int>(size));
    }

    void shutdown() {
        if (ssl != nullptr) {
            SSL_shutdown(ssl);
        }
    }
};

SecureConnector::SecureConnector(const SecureConfig& config)
    : is_reliable_(config.is_reliable), is_server_(config.is_server), socket_fd_(-1) {
    // Create the TLS session only for TCP connections. DTLS sessions are created
    // per peer once a new datagram connection is established.
    if (is_reliable_) {
        tls_connections_[tcp_connector_] = std::make_unique<TlsSession>(false, is_server_);
        init_tls();
    }
}

SecureConnector::~SecureConnector() {
    if (is_reliable_ && !is_server_) {
        auto it = tls_connections_.find(tcp_connector_);
        if (it != tls_connections_.end() && it->second) {
            it->second->shutdown();
        }
    }
}

void SecureConnector::init_tls() {
    auto& conn = tls_connections_[tcp_connector_];
    if (conn) {
        conn->create_ssl();
    }
}

void SecureConnector::start_tls(const int& file_descriptor) {
    if (file_descriptor == -1) {
        LGSOMEIP_LOG_ERROR << "SecureConnector::start_tls / socket is invalid!";
        return;
    } else if (!is_reliable_) {
        LGSOMEIP_LOG_ERROR << "SecureConnector::start_tls / Use 'start_dtls' instead!";
    }

    LGSOMEIP_LOG_DEBUG << "SecureConnector::start_tls / " << (is_server_ ? "TCP Server," : "TCP Client,")
                       << "FD: " << file_descriptor;

    socket_fd_ = file_descriptor;
    auto& conn = tls_connections_[tcp_connector_];
    if (!conn) {
        return;
    }
    conn->set_fd(socket_fd_);

    if (conn->handshake() < 0) {
        LGSOMEIP_LOG_ERROR << "SecureConnector::start_tls / TLS Handshake fail!";
        return;
    }

    LGSOMEIP_LOG_DEBUG << "SecureConnector::start_tls / TLS " << (is_server_ ? "server accept" : "client connect")
                       << " done";
}

void SecureConnector::start_dtls(const int& file_descriptor, std::shared_ptr<Address> peer_address) {
    if (file_descriptor == -1) {
        LGSOMEIP_LOG_ERROR << "SecureConnector::start_dtls / socket is invalid!";
        return;
    } else if (is_reliable_) {
        LGSOMEIP_LOG_ERROR << "SecureConnector::start_dtls / Use 'start_tls' instead!";
        return;
    }

    std::string address_key = peer_address->get_ip_address() + ":" + std::to_string(peer_address->get_port_address());
    LGSOMEIP_LOG_DEBUG << "SecureConnector::start_dtls / " << (is_server_ ? "UDP Server," : "UDP Client,")
                       << "FD: " << file_descriptor << ", Peer:" << address_key;

    socket_fd_ = file_descriptor;
    auto new_tls = std::make_unique<TlsSession>(true, is_server_);
    if (!new_tls->create_ssl()) {
        LGSOMEIP_LOG_ERROR << "SecureConnector::start_dtls / failed to create DTLS session!";
        return;
    }

    new_tls->set_fd(socket_fd_);

    if (!is_server_) {
        // A connected UDP socket is required for the DTLS client handshake.
        if (::connect(socket_fd_, peer_address->get_address(), peer_address->get_address_size()) != 0) {
            LGSOMEIP_LOG_ERROR << "SecureConnector::start_dtls / Client connect failed! errno #" << errno;
            return;
        }
    }

    new_tls->set_peer_info(peer_address->get_address());

    if (new_tls->handshake() < 0) {
        LGSOMEIP_LOG_ERROR << "SecureConnector::start_dtls / DTLS handshake fail!";
        return;
    }

    LGSOMEIP_LOG_INFO << "SecureConnector::start_dtls / DTLS " << (is_server_ ? "server accept" : "client connect")
                      << " done";

    std::lock_guard<std::mutex> tls_map_lock(tls_map_mutex_);
    tls_connections_.insert(std::make_pair(address_key, std::move(new_tls)));
}

std::size_t SecureConnector::send(const void* buffer, std::size_t size, std::string address, std::uint16_t port) {
    std::size_t send_length = static_cast<std::size_t>(-1);

    std::lock_guard<std::mutex> tls_map_lock(tls_map_mutex_);
    if (is_reliable_) {
        auto& conn = tls_connections_[tcp_connector_];
        if (conn) {
            int written = conn->send(buffer, size);
            if (written >= 0) {
                send_length = static_cast<std::size_t>(written);
            }
        }
    } else {
        std::string address_key = address + ":" + std::to_string(port);
        auto tls_connection_it = tls_connections_.find(address_key);
        if (tls_connection_it != tls_connections_.end() && tls_connection_it->second) {
            int written = tls_connection_it->second->send(buffer, size);
            if (written >= 0) {
                send_length = static_cast<std::size_t>(written);
            }
        }
    }

    return send_length;
}

std::size_t SecureConnector::receive(char* buffer, std::size_t size, std::string address, std::uint16_t port) {
    int receive_length = -1;

    std::unique_lock<std::mutex> tls_map_lock(tls_map_mutex_);
    if (is_reliable_) {
        auto& conn = tls_connections_[tcp_connector_];
        if (conn) {
            receive_length = conn->recv(buffer, size);
        }
    } else {
        std::string address_key = address + ":" + std::to_string(port);
        auto tls_connection_it = tls_connections_.find(address_key);
        if (tls_connection_it != tls_connections_.end() && tls_connection_it->second) {
            receive_length = tls_connection_it->second->recv(buffer, size);
        }
    }
    tls_map_lock.unlock();

    return static_cast<std::size_t>(receive_length);
}

bool SecureConnector::is_new_dtls_connection(std::string address, std::uint16_t port) const {
    std::string address_key = address + ":" + std::to_string(port);

    std::lock_guard<std::mutex> tls_map_lock(tls_map_mutex_);
    return tls_connections_.find(address_key) == tls_connections_.end();
}

void SecureConnector::handle_disconnect(std::string address, std::uint16_t port) {
    std::string address_key = address + ":" + std::to_string(port);
    std::lock_guard<std::mutex> tls_map_lock(tls_map_mutex_);

    auto tls_connection_it = tls_connections_.find(address_key);
    if (tls_connection_it != tls_connections_.end()) {
        if (!is_reliable_ && !is_server_ && tls_connection_it->second) {
            tls_connection_it->second->shutdown();
        }
        tls_connections_.erase(tls_connection_it);
    }
}

bool SecureConnector::check_dtls_data_available(int file_descriptor, struct sockaddr* peer_address) {
    if (file_descriptor < 0 || peer_address == nullptr) {
        return false;
    }

    sockaddr_storage storage;
    std::memset(&storage, 0, sizeof(storage));
    socklen_t storage_length = sizeof(storage);
    char probe = 0;

    ssize_t peeked = ::recvfrom(file_descriptor, &probe, sizeof(probe), MSG_PEEK, reinterpret_cast<sockaddr*>(&storage),
                                &storage_length);
    if (peeked < 0) {
        return false;
    }

    std::size_t copy_length = 0;
    switch (storage.ss_family) {
    case AF_INET:
        copy_length = sizeof(sockaddr_in);
        break;
    case AF_INET6:
        copy_length = sizeof(sockaddr_in6);
        break;
    case AF_UNIX:
        copy_length = sizeof(sockaddr_un);
        break;
    default:
        copy_length = 0;
        break;
    }
    if (copy_length > 0) {
        std::memcpy(peer_address, &storage, copy_length);
    }

    return true;
}

} // namespace osabstraction
} // namespace lgsomeip
