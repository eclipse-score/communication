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

#include <iostream>
#include <sstream>
#include <bitset>
#include <cstdio>
#include <string>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ctime>

#if defined(QNX)
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <queue>
#include <libgen.h>
#endif

#define SOMEIP_DAEMON_UDS_NAME "/tmp/someip/someip-0000"
#define DUMP_SOMEIPSERVICES_UDS_NAME "/tmp/someip/dump-someipservices"

#if defined(QNX)
#define SOMEIP_DAEMON_MESSAGEPASSING_CHANNEL_NAME "someip/someip-0000"
#endif

constexpr bool is_little_endian(void) {
#if defined(LINUX)
    return __BYTE_ORDER == __LITTLE_ENDIAN;
#elif defined(QNX)
#if defined(__LITTLEENDIAN__)
    return true;
#else
    return false;
#endif
#else
    unsigned int i = 1;
    return (*reinterpret_cast<char*>(&i) == 0x01);
#endif
}

std::string get_current_time() {
    char buffer[26];
    struct tm* tm_info;
    struct timeval tv;
    std::string current_time;

    gettimeofday(&tv, NULL);

    tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);

    current_time = buffer + std::to_string(tv.tv_usec);

    return current_time;
}

int main(int argc, char* argv[]) {
    // SOME/IP-SD data to trigger dumping the current state of SOME/IP services
    std::string hex_chars("ff ff 81 00 00 00 00 20 00 00 00 00 00 00 02 00 00 00 00 00 00 00 00 10 f1 f2 00 00 00 00 "
                          "00 00 00 00 00 00 00 00 00 00");
    std::istringstream hex_chars_stream(hex_chars);
    std::vector<unsigned char> bytes;

    // Create the trigger message
    unsigned int byte_value;

    // Add the header of the trigger message
    while (hex_chars_stream >> std::hex >> byte_value) {
        bytes.push_back(byte_value);
    }

    // set the length of the trigger message
    bytes[7] = bytes.size() - 8;

#if !defined(QNX)
    std::cout << get_current_time() << ": Try to send the trigger message via Unix domain socket to "
              << SOMEIP_DAEMON_UDS_NAME << std::endl;

    int socket_fd;
    struct sockaddr_un someip_daemon_addr {
    }, dump_someipservices_addr{};

    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    dump_someipservices_addr.sun_family = PF_UNIX;
    std::snprintf(dump_someipservices_addr.sun_path, sizeof(dump_someipservices_addr.sun_path), "%s",
                  DUMP_SOMEIPSERVICES_UDS_NAME);

    someip_daemon_addr.sun_family = PF_UNIX;
    std::snprintf(someip_daemon_addr.sun_path, sizeof(someip_daemon_addr.sun_path), "%s", SOMEIP_DAEMON_UDS_NAME);

    // remove the previous UDS file of this application
    if (access(dump_someipservices_addr.sun_path, F_OK) == 0) {
        unlink(dump_someipservices_addr.sun_path);
    }

    if (bind(socket_fd, (struct sockaddr*)&dump_someipservices_addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (connect(socket_fd, (struct sockaddr*)&someip_daemon_addr, sizeof(struct sockaddr_un)) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    // Nagle Algorithm Off
    int option = 1;
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));

    // Send a trigger message to make the someip-daemon create the files
    // including the current subscription/providing states of SOME/IP services.
    const ssize_t bytes_written = write(socket_fd, (const char*)bytes.data(), bytes.size());
    if (bytes_written != static_cast<ssize_t>(bytes.size())) {
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }

    // Sleep some time before closing the socket to ensure that the trigger message is sent successfully.
    sleep(1);

    close(socket_fd);
    unlink(dump_someipservices_addr.sun_path);

#else
    std::cout << get_current_time() << ": Try to send the trigger message via Message Passing." << std::endl;

    constexpr std::uint16_t kMessageData = _IO_MAX + 1;
    constexpr std::uint16_t kMessageConnect = _IO_MAX + 2;
    union MessagePassingIoHeader {
        std::uint16_t type;
        struct _pulse pulse;
    };
    struct _server_info server_info;
    int connection_id;

    std::string server_name = SOMEIP_DAEMON_MESSAGEPASSING_CHANNEL_NAME;
    std::string name_path = "/dev/name/local/";
    name_path += server_name;

    if (waitfor(name_path.c_str(), 100 * 1000, 100) != 0) {
        std::cerr << get_current_time() << ": Message-passing server was not found." << std::endl;
        return EXIT_FAILURE;
    }

    connection_id = name_open(server_name.data(), 0);

    if (connection_id != -1) {
        ConnectServerInfo(0, connection_id, &server_info);

        MessagePassingIoHeader header;
        iov_t iov[2];

        // Send connection info
        SETIOV(&iov[0], &header, sizeof(header.type));

        header.type = kMessageConnect;
        MsgSendv(connection_id, iov, 1, NULL, 0);

        // Send data
        SETIOV(&iov[0], &header, sizeof(header.type));
        SETIOV(&iov[1], (void*)(bytes.data()), bytes.size());

        header.type = kMessageData;
        MsgSendv(connection_id, iov, 2, NULL, 0);
        name_close(connection_id);
    } else {
        std::cerr << get_current_time() << ": Cannot connect with messagepassing server." << std::endl;
        return EXIT_FAILURE;
    }

#endif

    std::cout << get_current_time() << ": Sent the trigger message to the someip-daemon." << std::endl;
    std::cout << get_current_time() << ": Please check the /tmp/someip/dumpservices folder." << std::endl;

    return 0;
}
