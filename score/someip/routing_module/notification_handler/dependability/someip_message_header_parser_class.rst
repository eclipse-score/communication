..
   # *******************************************************************************
   # Copyright (c) 2026 Contributors to the Eclipse Foundation
   #
   # See the NOTICE file(s) distributed with this work for additional
   # information regarding copyright ownership.
   #
   # This program and the accompanying materials are made available under the
   # terms of the Apache License Version 2.0 which is available at
   # https://www.apache.org/licenses/LICENSE-2.0
   #
   # SPDX-License-Identifier: Apache-2.0
   # *******************************************************************************

SOME/IP Header Parser
^^^^^^^^^^^^^^^^^^^^^

``SomeipMessageHeaderParser`` validates one fixed-size SOME/IP header and its declared length against the bytes remaining in a datagram. It does not copy packet data, filter message types, or apply routing state; the RX path performs those operations.

``Parse(header, bytes_remaining_in_datagram)`` returns the service and method/event IDs, client and session IDs, protocol and interface versions, message type, return code, total message size, and payload size. It returns an error for an incomplete header, unsupported protocol version, a length below eight, or a message extending past the remaining datagram bytes.

``SomeipDatagramParser`` is templated on a concrete packet reader, so ``Size()`` and ``Copy()`` calls are statically
dispatched. ``IoSockPacketReader`` forwards ``Copy()`` to the io-sock module's ``someip_iosock_copy_packet()``,
which uses ``m_copydata`` on the mbuf chain. For each message, the parser copies
only the fixed 16-byte header into stack storage. ``NextMessage()`` returns the header metadata and payload offset of
the next message of any message type; it does not copy or flatten the payload. ``NotificationHandler`` drops
non-notification messages and copies the payload of a selected notification directly from the mbuf chain into its
LoLa sample. When the datagram is exhausted the parser returns
an empty optional; malformed framing or a packet-read failure returns an error and stops iteration.

The length field is big endian and counts bytes after the length field. Thus the fixed eight-byte suffix (client ID, session ID, protocol version, interface version, message type, return code) is subtracted to obtain the payload size. An empty payload is valid.

Wire format
"""""""""""

All multi-byte fields are unsigned and encoded in network byte order (big endian). The fixed header is 16 bytes;
the payload, if any, starts at offset 16.

.. list-table:: SOME/IP message header
   :header-rows: 1
   :widths: 8 10 22 60

   * - Offset
     - Size
     - Field
     - Meaning
   * - 0
     - 2
     - Service ID
     - Identifies the service.
   * - 2
     - 2
     - Method ID / Event ID
     - Identifies the method or event; for notifications this is the Event ID.
   * - 4
     - 4
     - Length
     - Number of bytes from offset 8 to message end: the 8-byte suffix plus payload.
   * - 8
     - 2
     - Client ID
     - First half of the Request ID.
   * - 10
     - 2
     - Session ID
     - Second half of the Request ID.
   * - 12
     - 1
     - Protocol Version
     - This parser accepts version ``1``.
   * - 13
     - 1
     - Interface Version
     - Service interface major version.
   * - 14
     - 1
     - Message Type
     - ``0x02`` denotes Notification; the parser returns other types for filtering.
   * - 15
     - 1
     - Return Code
     - Message return code.
   * - 16
     - variable
     - Payload
     - ``Length - 8`` bytes. A zero-length payload is valid.

The total message size is ``8 + Length`` bytes. A Length smaller than 8 is malformed, and the declared total must
not exceed the bytes remaining in the UDP datagram. Concatenated messages are parsed by advancing by this total
size for each message.

.. uml:: someip_message_header_parser_class.puml
   :align: center
   :alt: SOME/IP Header Parser Class
   :width: 100%

Sequences
"""""""""

.. uml:: someip_datagram_parser_sequence.puml
  :align: center
  :alt: SOME/IP datagram notification parsing and payload delivery
