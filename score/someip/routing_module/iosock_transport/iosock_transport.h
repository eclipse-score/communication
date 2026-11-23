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
 ********************************************************************************/
#ifndef SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_IOSOCK_TRANSPORT_H
#define SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_IOSOCK_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented by the io-sock module. Except for someip_iosock_wakeup(), call on the module's kernel thread only. */

/* Opens a kernel UDP socket on the port for all local IPv4 addresses; an open port succeeds. Returns 0 or an errno. */
int someip_iosock_open_udp_socket(unsigned short port);

/* Copies size bytes at offset of a received packet into destination; the range must lie within the packet. */
void someip_iosock_copy_packet(const void* packet, unsigned int offset, unsigned int size, void* destination);

/* Wakes the module's kernel thread; callable from any thread. */
void someip_iosock_wakeup(void);

/* Implemented by the routing module and called on the module's kernel thread. */

/* Returns 0 or an errno, which fails loading the module. */
int someip_routing_start(void);

/* Undoes someip_routing_start() when the kernel thread cannot be created. */
void someip_routing_abort_start(void);

/* Processes pending routing work; called before received datagrams are handled. */
void someip_routing_poll(void);

/* packet is the mbuf chain of a datagram with at least a SOME/IP header; valid only during the call. */
void someip_routing_datagram(unsigned short local_port,
                             const unsigned char sender_ip[4],
                             unsigned short sender_port,
                             const void* packet,
                             unsigned int size);

#ifdef __cplusplus
}
#endif

#endif /* SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_IOSOCK_TRANSPORT_H */
