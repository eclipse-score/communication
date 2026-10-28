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
 *
 ********************************************************************************/

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <unistd.h>

#include <deque>

#include <endpoint/EndpointTCP.h>

using namespace lgsomeip;

struct EndpointTCPTestHost {
    void on_message(std::shared_ptr<Endpoint>, std::uint8_t* message, std::size_t length) {
        received.assign(message, message + length);
    }
    void on_disconnect(std::shared_ptr<Endpoint>) {
        ++disconnect_count;
    }
    void on_connect(std::shared_ptr<Endpoint>, std::shared_ptr<Endpoint>) {}

    std::vector<std::uint8_t> received;
    int disconnect_count{0};
};

// A socket whose incoming bytes are supplied by the test. Like TCPSocket::receive(), receive() returns as many
// of the requested bytes as are available, which is zero when nothing has arrived.
class FeedSocket : public osabstraction::Socket {
public:
    explicit FeedSocket(std::shared_ptr<osabstraction::Address> address = std::make_shared<osabstraction::IP4Address>())
        : osabstraction::Socket(-1, address) {}

    void feed(const std::vector<std::uint8_t>& bytes) {
        pending_.insert(pending_.end(), bytes.begin(), bytes.end());
    }

    int receive(char* buffer, std::size_t size) override {
        std::size_t len = std::min(size, pending_.size());
        std::copy_n(pending_.begin(), len, buffer);
        pending_.erase(pending_.begin(), pending_.begin() + len);
        return static_cast<int>(len);
    }

private:
    std::deque<std::uint8_t> pending_;
};

const std::vector<std::uint8_t> kMessage = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x01,
                                            0x00, 0x02, 0x01, 0x01, 0x00, 0x00, 0xde, 0xad, 0xbe, 0xef};

std::vector<std::uint8_t> slice(const std::vector<std::uint8_t>& bytes, std::size_t from, std::size_t to) {
    return std::vector<std::uint8_t>(bytes.begin() + from, bytes.begin() + to);
}

// Delivers the stream in pieces of chunk bytes, calling the endpoint after each piece, as the multiplexer would.
template <typename EndpointT>
void feed_in_chunks(FeedSocket& socket, EndpointT& endpoint, const std::vector<std::uint8_t>& stream,
                    std::size_t chunk) {
    for (std::size_t pos = 0; pos < stream.size(); pos += chunk) {
        socket.feed(slice(stream, pos, std::min(pos + chunk, stream.size())));
        endpoint.callback(false);
    }
}

TEST(EndpointTCPClient, recognizes_server_and_client_magic_cookies) {
    EndpointTCPClient<EndpointTCPTestHost> endpoint(nullptr);
    std::uint8_t invalid_cookie[kLenMagicCookie] = {};

    EXPECT_TRUE(endpoint.is_magic_cookie(const_cast<std::uint8_t*>(kServerMagicCookie), kLenMagicCookie));
    EXPECT_TRUE(endpoint.is_magic_cookie(const_cast<std::uint8_t*>(kClientMagicCookie), kLenMagicCookie));
    EXPECT_FALSE(endpoint.is_magic_cookie(invalid_cookie, sizeof(invalid_cookie)));
}

TEST(EndpointTCPClient, stores_server_endpoint_reference) {
    EndpointTCPClient<EndpointTCPTestHost> client(nullptr);
    auto server = std::make_shared<EndpointTCPServer<EndpointTCPTestHost>>(nullptr);

    client.set_server_endpoint(server);

    EXPECT_EQ(client.get_server_endpoint(), server);
}

TEST(EndpointTCPServer, manages_client_file_descriptors) {
    EndpointTCPServer<EndpointTCPTestHost> server(nullptr);

    server.add_client_endpoint(10);
    server.add_client_endpoint(11);
    ASSERT_EQ(server.get_client_list(), (std::vector<std::int32_t>{10, 11}));

    server.remove_client_endpoint(10);
    EXPECT_EQ(server.get_client_list(), (std::vector<std::int32_t>{11}));
    server.remove_client_endpoint(99);
    EXPECT_EQ(server.get_client_list(), (std::vector<std::int32_t>{11}));
}

TEST(EndpointTCPClient, reassembles_message_received_in_small_chunks) {
    for (std::size_t chunk : {1, 3, 5, 7}) {
        EndpointTCPTestHost host;
        auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
        auto socket = std::make_shared<FeedSocket>();
        endpoint->set_socket(socket);

        feed_in_chunks(*socket, *endpoint, kMessage, chunk);

        EXPECT_EQ(host.received, kMessage) << "chunk size " << chunk;
    }
}

TEST(EndpointTCPClient, keeps_partial_message_until_the_rest_arrives) {
    // Every split point: inside the header, at the header end, inside the body, and one byte before the end.
    for (std::size_t split = 1; split < kMessage.size(); ++split) {
        EndpointTCPTestHost host;
        auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
        auto socket = std::make_shared<FeedSocket>();
        endpoint->set_socket(socket);

        socket->feed(slice(kMessage, 0, split));
        endpoint->callback(false);
        EXPECT_TRUE(host.received.empty()) << "split " << split;
        EXPECT_EQ(host.disconnect_count, 0) << "split " << split;

        // Nothing new has arrived; the partial message must survive an empty wake-up.
        endpoint->callback(false);
        EXPECT_TRUE(host.received.empty()) << "split " << split;

        socket->feed(slice(kMessage, split, kMessage.size()));
        endpoint->callback(false);
        EXPECT_EQ(host.received, kMessage) << "split " << split;
        EXPECT_EQ(host.disconnect_count, 0) << "split " << split;
    }
}

TEST(EndpointTCPClient, does_not_consume_bytes_of_the_following_message) {
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    auto socket = std::make_shared<FeedSocket>();
    endpoint->set_socket(socket);

    std::vector<std::uint8_t> second = kMessage;
    second[0] = 0x56;
    std::vector<std::uint8_t> stream = kMessage;
    stream.insert(stream.end(), second.begin(), second.end());
    socket->feed(stream);

    endpoint->callback(false);
    EXPECT_EQ(host.received, kMessage);

    endpoint->callback(false);
    EXPECT_EQ(host.received, second);
    EXPECT_EQ(host.disconnect_count, 0);
}

TEST(EndpointTCPClient, receives_message_larger_than_static_buffer_in_pieces) {
    const std::size_t payload_size = EndpointBase::STATIC_BUFFER_SIZE * 2 + 5;
    std::vector<std::uint8_t> message = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x00, 0x00};
    const std::uint32_t length = static_cast<std::uint32_t>(8 + payload_size);
    message[4] = length >> 24;
    message[5] = length >> 16;
    message[6] = length >> 8;
    message[7] = length;
    for (std::size_t i = 0; i < payload_size; ++i) {
        message.push_back(static_cast<std::uint8_t>(i));
    }

    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    auto socket = std::make_shared<FeedSocket>();
    endpoint->set_socket(socket);

    socket->feed(slice(message, 0, 10));
    endpoint->callback(false);
    socket->feed(slice(message, 10, message.size() / 2));
    endpoint->callback(false);
    EXPECT_TRUE(host.received.empty());

    socket->feed(slice(message, message.size() / 2, message.size()));
    endpoint->callback(false);
    EXPECT_EQ(host.received, message);
}

TEST(EndpointTCPClient, reads_instance_id_of_internal_message_in_small_chunks) {
    std::vector<std::uint8_t> stream = kMessage;
    stream.push_back(0x12);
    stream.push_back(0x34);

    for (std::size_t chunk : {1, 3}) {
        EndpointTCPTestHost host;
        auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
        auto socket = std::make_shared<FeedSocket>(std::make_shared<osabstraction::LocalAddress>());
        endpoint->set_socket(socket);

        feed_in_chunks(*socket, *endpoint, stream, chunk);

        EXPECT_EQ(host.received, kMessage) << "chunk size " << chunk;
        EXPECT_EQ(endpoint->get_instance_id(), 0x1234) << "chunk size " << chunk;
    }
}

TEST(EndpointTCPClient, waits_for_instance_id_that_arrives_late) {
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    auto socket = std::make_shared<FeedSocket>(std::make_shared<osabstraction::LocalAddress>());
    endpoint->set_socket(socket);

    socket->feed(kMessage);
    socket->feed({0x12});
    endpoint->callback(false);
    EXPECT_TRUE(host.received.empty());

    socket->feed({0x34});
    endpoint->callback(false);
    EXPECT_EQ(host.received, kMessage);
    EXPECT_EQ(endpoint->get_instance_id(), 0x1234);
}

TEST(EndpointTCPClient, disconnects_when_length_field_is_invalid) {
    const std::vector<std::uint8_t> zero_length = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    const std::vector<std::uint8_t> short_length = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01};
    const std::vector<std::uint8_t> huge_length = {0x12, 0x34, 0x80, 0x01, 0xfd, 0x00, 0x00, 0xff, 0x00, 0x01};

    for (const auto& stream : {zero_length, short_length, huge_length}) {
        EndpointTCPTestHost host;
        auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
        auto socket = std::make_shared<FeedSocket>();
        endpoint->set_socket(socket);

        feed_in_chunks(*socket, *endpoint, stream, 4);

        EXPECT_EQ(host.disconnect_count, 1);
        EXPECT_TRUE(host.received.empty());
    }
}

TEST(EndpointTCPClient, drops_partial_message_when_connection_closes) {
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    auto socket = std::make_shared<FeedSocket>();
    endpoint->set_socket(socket);

    socket->feed(slice(kMessage, 0, 12));
    endpoint->callback(false);
    endpoint->callback(true);
    EXPECT_EQ(host.disconnect_count, 1);

    // A reused endpoint starts with a fresh header, not in the middle of the old message.
    socket->feed(kMessage);
    endpoint->callback(false);
    EXPECT_EQ(host.received, kMessage);
}

namespace {

// Two connected stream sockets, so that the real socket code (non-blocking reads, end of stream) is exercised.
struct SocketPair {
    SocketPair() {
        int fds[2];
        EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
        auto address = std::make_shared<osabstraction::IP4Address>();
        address->set_reliable(true);
        endpoint_socket = std::make_shared<osabstraction::TCPSocket>(fds[0], address);
        peer_fd = fds[1];
    }
    ~SocketPair() {
        if (peer_fd >= 0) {
            ::close(peer_fd);
        }
    }
    void write(const std::vector<std::uint8_t>& bytes) {
        ASSERT_EQ(static_cast<ssize_t>(bytes.size()), ::write(peer_fd, bytes.data(), bytes.size()));
    }
    void close_peer() {
        ::close(peer_fd);
        peer_fd = -1;
    }

    std::shared_ptr<osabstraction::TCPSocket> endpoint_socket;
    int peer_fd{-1};
};

} // namespace

TEST(EndpointTCPClient, receives_message_written_in_two_steps_on_a_real_socket) {
    SocketPair pair;
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    endpoint->set_socket(pair.endpoint_socket);

    pair.write(slice(kMessage, 0, 5));
    endpoint->callback(false);
    EXPECT_TRUE(host.received.empty());
    EXPECT_EQ(host.disconnect_count, 0);

    pair.write(slice(kMessage, 5, kMessage.size()));
    endpoint->callback(false);
    EXPECT_EQ(host.received, kMessage);
}

TEST(EndpointTCPClient, disconnects_when_peer_closes_the_stream) {
    SocketPair pair;
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    endpoint->set_socket(pair.endpoint_socket);

    // A readable socket without data is reported as ready once the peer has closed.
    pair.close_peer();
    endpoint->callback(false);

    EXPECT_EQ(host.disconnect_count, 1);
    EXPECT_TRUE(host.received.empty());
}

TEST(EndpointTCPClient, disconnects_when_peer_closes_in_the_middle_of_a_message) {
    SocketPair pair;
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    endpoint->set_socket(pair.endpoint_socket);

    pair.write(slice(kMessage, 0, 12));
    pair.close_peer();
    endpoint->callback(false);

    EXPECT_EQ(host.disconnect_count, 1);
    EXPECT_TRUE(host.received.empty());
}

TEST(EndpointTCPClient, does_not_disconnect_when_no_data_is_available_yet) {
    SocketPair pair;
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    endpoint->set_socket(pair.endpoint_socket);

    endpoint->callback(false);

    EXPECT_EQ(host.disconnect_count, 0);
}
