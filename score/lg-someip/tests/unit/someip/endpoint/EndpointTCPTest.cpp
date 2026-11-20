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

#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>

#include <endpoint/EndpointTCP.h>
#include <multiplex/Multiplexer.h>

#include "test_utils.h"

using namespace lgsomeip;

struct EndpointTCPTestHost
{
    void on_message(std::shared_ptr<Endpoint>, std::uint8_t* message, std::size_t length)
    {
        received.assign(message, message + length);
    }
    void on_disconnect(std::shared_ptr<Endpoint>)
    {
        ++disconnect_count;
    }
    void on_connect(std::shared_ptr<Endpoint>, std::shared_ptr<Endpoint>) {}

    std::vector<std::uint8_t> received;
    int disconnect_count{0};
};

// A socket whose incoming bytes are supplied by the test. Like TCPSocket::receive(), receive() returns as many
// of the requested bytes as are available, which is zero when nothing has arrived.
class FeedSocket : public osabstraction::Socket
{
  public:
    explicit FeedSocket(std::shared_ptr<osabstraction::Address> address = std::make_shared<osabstraction::IP4Address>())
        : osabstraction::Socket(-1, address)
    {
    }

    void feed(const std::vector<std::uint8_t>& bytes)
    {
        pending_.insert(pending_.end(), bytes.begin(), bytes.end());
    }

    int receive(char* buffer, std::size_t size) override
    {
        std::size_t len = std::min(size, pending_.size());
        std::copy_n(pending_.begin(), len, buffer);
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(len));
        return static_cast<int>(len);
    }

  private:
    std::deque<std::uint8_t> pending_;
};

const std::vector<std::uint8_t> kMessage = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x01,
                                            0x00, 0x02, 0x01, 0x01, 0x00, 0x00, 0xde, 0xad, 0xbe, 0xef};

std::vector<std::uint8_t> slice(const std::vector<std::uint8_t>& bytes, std::size_t from, std::size_t to)
{
    return std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(from),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(to));
}

// Delivers the stream in pieces of chunk bytes, calling the endpoint after each piece, as the multiplexer would.
template <typename EndpointT>
void feed_in_chunks(FeedSocket& socket, EndpointT& endpoint, const std::vector<std::uint8_t>& stream, std::size_t chunk)
{
    for (std::size_t pos = 0; pos < stream.size(); pos += chunk)
    {
        socket.feed(slice(stream, pos, std::min(pos + chunk, stream.size())));
        endpoint.callback(false);
    }
}

TEST(EndpointTCPClient, recognizes_server_and_client_magic_cookies)
{
    EndpointTCPClient<EndpointTCPTestHost> endpoint(nullptr);
    std::uint8_t invalid_cookie[kLenMagicCookie] = {};

    EXPECT_TRUE(endpoint.is_magic_cookie(const_cast<std::uint8_t*>(kServerMagicCookie), kLenMagicCookie));
    EXPECT_TRUE(endpoint.is_magic_cookie(const_cast<std::uint8_t*>(kClientMagicCookie), kLenMagicCookie));
    EXPECT_FALSE(endpoint.is_magic_cookie(invalid_cookie, sizeof(invalid_cookie)));
}

TEST(EndpointTCPClient, stores_server_endpoint_reference)
{
    EndpointTCPClient<EndpointTCPTestHost> client(nullptr);
    auto server = std::make_shared<EndpointTCPServer<EndpointTCPTestHost>>(nullptr);

    client.set_server_endpoint(server);

    EXPECT_EQ(client.get_server_endpoint(), server);
}

TEST(EndpointTCPServer, manages_client_file_descriptors)
{
    EndpointTCPServer<EndpointTCPTestHost> server(nullptr);

    server.add_client_endpoint(10);
    server.add_client_endpoint(11);
    ASSERT_EQ(server.get_client_list(), (std::vector<std::int32_t>{10, 11}));

    server.remove_client_endpoint(10);
    EXPECT_EQ(server.get_client_list(), (std::vector<std::int32_t>{11}));
    server.remove_client_endpoint(99);
    EXPECT_EQ(server.get_client_list(), (std::vector<std::int32_t>{11}));
}

TEST(EndpointTCPClient, reassembles_message_received_in_small_chunks)
{
    for (std::size_t chunk : {1U, 3U, 5U, 7U})
    {
        EndpointTCPTestHost host;
        auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
        auto socket = std::make_shared<FeedSocket>();
        endpoint->set_socket(socket);

        feed_in_chunks(*socket, *endpoint, kMessage, chunk);

        EXPECT_EQ(host.received, kMessage) << "chunk size " << chunk;
    }
}

TEST(EndpointTCPClient, keeps_partial_message_until_the_rest_arrives)
{
    // Every split point: inside the header, at the header end, inside the body, and one byte before the end.
    for (std::size_t split = 1; split < kMessage.size(); ++split)
    {
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

TEST(EndpointTCPClient, does_not_consume_bytes_of_the_following_message)
{
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

TEST(EndpointTCPClient, receives_message_larger_than_static_buffer_in_pieces)
{
    const std::size_t payload_size = EndpointBase::STATIC_BUFFER_SIZE * 2 + 5;
    std::vector<std::uint8_t> message = {
        0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x00, 0x00};
    const std::uint32_t length = static_cast<std::uint32_t>(8 + payload_size);
    message[4] = static_cast<std::uint8_t>(length >> 24);
    message[5] = static_cast<std::uint8_t>(length >> 16);
    message[6] = static_cast<std::uint8_t>(length >> 8);
    message[7] = static_cast<std::uint8_t>(length);
    for (std::size_t i = 0; i < payload_size; ++i)
    {
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

TEST(EndpointTCPClient, reads_instance_id_of_internal_message_in_small_chunks)
{
    std::vector<std::uint8_t> stream = kMessage;
    stream.push_back(0x12);
    stream.push_back(0x34);

    for (std::size_t chunk : {1U, 3U})
    {
        EndpointTCPTestHost host;
        auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
        auto socket = std::make_shared<FeedSocket>(std::make_shared<osabstraction::LocalAddress>());
        endpoint->set_socket(socket);

        feed_in_chunks(*socket, *endpoint, stream, chunk);

        EXPECT_EQ(host.received, kMessage) << "chunk size " << chunk;
        EXPECT_EQ(endpoint->get_instance_id(), 0x1234) << "chunk size " << chunk;
    }
}

TEST(EndpointTCPClient, waits_for_instance_id_that_arrives_late)
{
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

TEST(EndpointTCPClient, disconnects_when_length_field_is_invalid)
{
    const std::vector<std::uint8_t> zero_length = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    const std::vector<std::uint8_t> short_length = {0x12, 0x34, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01};
    const std::vector<std::uint8_t> huge_length = {0x12, 0x34, 0x80, 0x01, 0xfd, 0x00, 0x00, 0xff, 0x00, 0x01};

    for (const auto& stream : {zero_length, short_length, huge_length})
    {
        EndpointTCPTestHost host;
        auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
        auto socket = std::make_shared<FeedSocket>();
        endpoint->set_socket(socket);

        feed_in_chunks(*socket, *endpoint, stream, 4);

        EXPECT_EQ(host.disconnect_count, 1);
        EXPECT_TRUE(host.received.empty());
    }
}

TEST(EndpointTCPClient, drops_partial_message_when_connection_closes)
{
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

namespace
{

// Two connected stream sockets, so that the real socket code (non-blocking reads, end of stream) is exercised.
struct SocketPair
{
    // An unreliable address makes every TCPSocket::receive() on the endpoint socket fail.
    explicit SocketPair(bool reliable = true)
    {
        int fds[2] = {-1, -1};
        const int result = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
        EXPECT_EQ(0, result);
        if (result != 0)
        {
            return;
        }
        auto address = std::make_shared<osabstraction::IP4Address>();
        address->set_reliable(reliable);
        endpoint_socket = std::make_shared<osabstraction::TCPSocket>(fds[0], address);
        peer_fd = fds[1];
    }
    ~SocketPair()
    {
        if (peer_fd >= 0)
        {
            ::close(peer_fd);
        }
    }
    void write(const std::vector<std::uint8_t>& bytes)
    {
        ASSERT_EQ(static_cast<ssize_t>(bytes.size()), ::write(peer_fd, bytes.data(), bytes.size()));
    }
    void close_peer()
    {
        ::close(peer_fd);
        peer_fd = -1;
    }

    std::shared_ptr<osabstraction::TCPSocket> endpoint_socket;
    int peer_fd{-1};
};

}  // namespace

TEST(EndpointTCPClient, receives_message_written_in_two_steps_on_a_real_socket)
{
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

TEST(EndpointTCPClient, disconnects_when_peer_closes_the_stream)
{
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

TEST(EndpointTCPClient, disconnects_when_peer_closes_in_the_middle_of_a_message)
{
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

TEST(EndpointTCPClient, does_not_disconnect_when_no_data_is_available_yet)
{
    SocketPair pair;
    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    endpoint->set_socket(pair.endpoint_socket);

    endpoint->callback(false);

    EXPECT_EQ(host.disconnect_count, 0);
}

namespace
{

constexpr std::chrono::seconds kEventTimeout{5};

test_utils::Clock::time_point event_deadline()
{
    return test_utils::Clock::now() + kEventTimeout;
}

// A host whose callbacks may run on the multiplexer thread, so every counter is atomic.
struct EndpointTCPCountingHost
{
    void on_message(std::shared_ptr<Endpoint>, std::uint8_t*, std::size_t)
    {
        ++message_count;
    }
    void on_disconnect(std::shared_ptr<Endpoint> endpoint)
    {
        if (endpoint->get_multiplexer() != nullptr)
        {
            listening_at_disconnect = true;
        }
        ++disconnect_count;
    }
    void on_connect(std::shared_ptr<Endpoint>, std::shared_ptr<Endpoint> client)
    {
        last_connected_fd = client->get_socket()->get_socket_fd();
        ++connect_count;
    }

    std::atomic<int> message_count{0};
    std::atomic<int> disconnect_count{0};
    std::atomic<int> connect_count{0};
    std::atomic<int> last_connected_fd{-2};
    std::atomic_bool listening_at_disconnect{false};
};

// A started Multiplexer that the case owns.
// The destructor stops and joins it.
// Declared after the endpoints it serves, so no endpoint callback runs while an endpoint is destroyed.
class RunningMultiplexer
{
  public:
    RunningMultiplexer() : multiplexer_{std::make_shared<osabstraction::Multiplexer>()}
    {
        multiplexer_->start();
        multiplexer_->set(probe_.ReadFd(), [this](bool) {
            test_utils::DrainByte(probe_.ReadFd());
            ++probe_calls_;
        });
    }
    ~RunningMultiplexer()
    {
        multiplexer_->stop();
        multiplexer_->join();
        multiplexer_->unset(probe_.ReadFd());
    }

    std::shared_ptr<osabstraction::Multiplexer> get() const
    {
        return multiplexer_;
    }

    // Pokes a probe fd set on the same multiplexer and waits for its callback.
    // Every fd that is readable and still set is dispatched in that pass too.
    bool completes_pass()
    {
        const int before{probe_calls_.load()};
        test_utils::PokeByte(probe_.WriteFd());
        return test_utils::WaitFor(
            [this, before]() {
                return probe_calls_.load() > before;
            },
            event_deadline());
    }

  private:
    test_utils::SocketPair probe_{open_pair()};
    std::atomic<int> probe_calls_{0};
    std::shared_ptr<osabstraction::Multiplexer> multiplexer_;

    static test_utils::SocketPair open_pair()
    {
        test_utils::SocketPair pair{};
        EXPECT_TRUE(pair.Open());
        return pair;
    }
};

std::shared_ptr<osabstraction::IP4Address> loopback_address()
{
    auto address = std::make_shared<osabstraction::IP4Address>();
    address->set_ip_address("127.0.0.1");
    address->set_port_address(0);
    address->set_reliable(true);
    return address;
}

bool wait_readable(int file_descriptor)
{
    pollfd descriptor{file_descriptor, POLLIN, 0};
    return ::poll(&descriptor, 1, static_cast<int>(std::chrono::milliseconds{kEventTimeout}.count())) > 0;
}

}  // namespace

// A closed peer reaches the host once, and the endpoint leaves the multiplexer before it tells the host.
// The hosts unset a provider-side endpoint only when its reference count reaches zero, so the endpoint unsets itself.
TEST(EndpointTCPClient, provider_side_client_disconnects_once_and_leaves_the_multiplexer_when_the_peer_closes)
{
    SocketPair pair;
    EndpointTCPCountingHost host;
    auto server = std::make_shared<EndpointTCPServer<EndpointTCPCountingHost>>(&host);
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPCountingHost>>(&host);
    endpoint->set_socket(pair.endpoint_socket);
    endpoint->set_server_endpoint(server);
    RunningMultiplexer multiplexer;
    endpoint->start_listen(multiplexer.get());

    pair.close_peer();
    ASSERT_TRUE(test_utils::WaitFor(
        [&host]() {
            return host.disconnect_count.load() >= 1;
        },
        event_deadline()))
        << "the closed peer never reached the host";

    // The socket stays readable at end of stream, so an fd still set would be dispatched in these passes.
    ASSERT_TRUE(multiplexer.completes_pass());
    ASSERT_TRUE(multiplexer.completes_pass());
    EXPECT_EQ(host.disconnect_count.load(), 1) << "the endpoint was dispatched again after its disconnect";
    EXPECT_FALSE(host.listening_at_disconnect.load()) << "the host was told before the endpoint left the multiplexer";
    EXPECT_EQ(endpoint->get_multiplexer(), nullptr);
}

// Every failed read closes the connection, so a socket that cannot be read reaches the host once.
TEST(EndpointTCPClient, disconnects_once_and_leaves_the_multiplexer_when_the_read_fails)
{
    // TCPSocket::receive() fails on a socket around an unreliable address.
    SocketPair pair{false};
    EndpointTCPCountingHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPCountingHost>>(&host);
    endpoint->set_socket(pair.endpoint_socket);
    RunningMultiplexer multiplexer;
    endpoint->start_listen(multiplexer.get());

    // The failing read consumes nothing, so the socket stays readable for the whole case.
    pair.write({0x01});
    ASSERT_TRUE(test_utils::WaitFor(
        [&host]() {
            return host.disconnect_count.load() >= 1;
        },
        event_deadline()))
        << "the failed read never reached the host";

    ASSERT_TRUE(multiplexer.completes_pass());
    ASSERT_TRUE(multiplexer.completes_pass());
    EXPECT_EQ(host.disconnect_count.load(), 1) << "the endpoint was dispatched again after its disconnect";
    EXPECT_FALSE(host.listening_at_disconnect.load()) << "the host was told before the endpoint left the multiplexer";
}

// A connection reset fails the read at once, and that read disconnects without waiting for end of stream.
TEST(EndpointTCPClient, disconnects_when_the_peer_resets_the_connection)
{
    sockaddr_in listener_address{};
    test_utils::UniqueFd listener{test_utils::MakeTcpListener(listener_address)};
    ASSERT_TRUE(listener.IsValid());
    test_utils::UniqueFd peer{::socket(AF_INET, SOCK_STREAM, 0)};
    ASSERT_TRUE(peer.IsValid());
    ASSERT_EQ(0, ::connect(peer.Get(), reinterpret_cast<const sockaddr*>(&listener_address), sizeof(listener_address)));
    const int accepted_fd{::accept(listener.Get(), nullptr, nullptr)};
    ASSERT_GE(accepted_fd, 0);

    EndpointTCPTestHost host;
    auto endpoint = std::make_shared<EndpointTCPClient<EndpointTCPTestHost>>(&host);
    endpoint->set_socket(std::make_shared<osabstraction::TCPSocket>(accepted_fd, loopback_address()));

    // A zero linger time makes close() send a reset instead of a FIN.
    const linger reset_on_close{1, 0};
    ASSERT_EQ(0, ::setsockopt(peer.Get(), SOL_SOCKET, SO_LINGER, &reset_on_close, sizeof(reset_on_close)));
    peer.Close();
    ASSERT_TRUE(wait_readable(accepted_fd)) << "the reset never arrived";

    endpoint->callback(false);

    EXPECT_EQ(host.disconnect_count, 1);
    EXPECT_TRUE(host.received.empty());
}

// A wake of the listening socket without a pending connection makes accept() fail, and no client reaches the host.
TEST(EndpointTCPServer, does_not_call_on_connect_when_accept_fails)
{
    auto listening_socket = std::make_shared<osabstraction::TCPServerSocket>(loopback_address());
    ASSERT_EQ(0, listening_socket->listen());
    // Non-blocking, so accept() without a pending connection fails at once instead of waiting for one.
    const int flags{::fcntl(listening_socket->get_socket_fd(), F_GETFL, 0)};
    ASSERT_EQ(0, ::fcntl(listening_socket->get_socket_fd(), F_SETFL, flags | O_NONBLOCK));

    EndpointTCPCountingHost host;
    auto server = std::make_shared<EndpointTCPServer<EndpointTCPCountingHost>>(&host);
    server->set_socket(listening_socket);

    server->callback(false);

    EXPECT_EQ(host.connect_count.load(), 0) << "a failed accept() reached the host as fd " << host.last_connected_fd;
    EXPECT_TRUE(server->get_client_list().empty());
}

// A pending connection reaches the host once, with the accepted socket.
TEST(EndpointTCPServer, calls_on_connect_once_for_a_pending_connection)
{
    auto listening_socket = std::make_shared<osabstraction::TCPServerSocket>(loopback_address());
    ASSERT_EQ(0, listening_socket->listen());
    sockaddr_in bound{};
    socklen_t bound_size{sizeof(bound)};
    ASSERT_EQ(0, ::getsockname(listening_socket->get_socket_fd(), reinterpret_cast<sockaddr*>(&bound), &bound_size));

    test_utils::UniqueFd peer{::socket(AF_INET, SOCK_STREAM, 0)};
    ASSERT_TRUE(peer.IsValid());
    ASSERT_EQ(0, ::connect(peer.Get(), reinterpret_cast<const sockaddr*>(&bound), sizeof(bound)));
    ASSERT_TRUE(wait_readable(listening_socket->get_socket_fd())) << "the connection never became pending";

    EndpointTCPCountingHost host;
    auto server = std::make_shared<EndpointTCPServer<EndpointTCPCountingHost>>(&host);
    server->set_socket(listening_socket);

    server->callback(false);

    ASSERT_EQ(host.connect_count.load(), 1);
    EXPECT_GE(host.last_connected_fd.load(), 0);
    EXPECT_EQ(server->get_client_list(), (std::vector<std::int32_t>{host.last_connected_fd.load()}));
}
