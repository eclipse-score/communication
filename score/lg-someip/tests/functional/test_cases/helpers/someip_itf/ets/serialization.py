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

"""SOME/IP serialization primitives without external dependencies.

All types use the big-endian wire encoding of the SOME/IP protocol.

Supported types
---------------
Primitives  : Bool, Uint8, Uint16, Uint32, Sint8, Sint16, Sint32, Float32
Composites  : SomeIpPayload (struct base class)
Arrays      : SomeIpFixedSizeArray, SomeIpDynamicSizeArray
"""

import struct
from dataclasses import dataclass
from typing import Generic, List, Type, TypeVar

# ---------------------------------------------------------------------------
# Primitives
# ---------------------------------------------------------------------------


@dataclass
class Bool:
    """Boolean transported as a single unsigned byte (0 = False, 1 = True)."""

    value: bool = False

    def __len__(self) -> int:
        return 1

    def serialize(self) -> bytes:
        """Serialize to one byte, big-endian (False → 0x00, True → 0x01)."""
        return struct.pack(">B", int(self.value))

    @classmethod
    def deserialize(cls, payload: bytes) -> "Bool":
        """Deserialize from one byte. 0 is False, all other values are True."""
        (raw,) = struct.unpack(">B", payload[:1])
        return cls(value=bool(raw))


@dataclass
class Uint8:
    """Unsigned 8-bit integer."""

    value: int = 0

    def __len__(self) -> int:
        return 1

    def serialize(self) -> bytes:
        """Serialize to one unsigned byte."""
        return struct.pack(">B", self.value)

    @classmethod
    def deserialize(cls, payload: bytes) -> "Uint8":
        """Deserialize from one unsigned byte."""
        (value,) = struct.unpack(">B", payload[:1])
        return cls(value=value)


@dataclass
class Sint8:
    """Signed 8-bit integer."""

    value: int = 0

    def __len__(self) -> int:
        return 1

    def serialize(self) -> bytes:
        """Serialize to one signed byte."""
        return struct.pack(">b", self.value)

    @classmethod
    def deserialize(cls, payload: bytes) -> "Sint8":
        """Deserialize from one signed byte."""
        (value,) = struct.unpack(">b", payload[:1])
        return cls(value=value)


@dataclass
class Uint16:
    """Unsigned 16-bit integer, big-endian."""

    value: int = 0

    def __len__(self) -> int:
        return 2

    def serialize(self) -> bytes:
        """Serialize to two bytes, big-endian."""
        return struct.pack(">H", self.value)

    @classmethod
    def deserialize(cls, payload: bytes) -> "Uint16":
        """Deserialize from two big-endian bytes."""
        (value,) = struct.unpack(">H", payload[:2])
        return cls(value=value)


@dataclass
class Sint16:
    """Signed 16-bit integer, big-endian."""

    value: int = 0

    def __len__(self) -> int:
        return 2

    def serialize(self) -> bytes:
        """Serialize to two bytes, big-endian."""
        return struct.pack(">h", self.value)

    @classmethod
    def deserialize(cls, payload: bytes) -> "Sint16":
        """Deserialize from two big-endian bytes."""
        (value,) = struct.unpack(">h", payload[:2])
        return cls(value=value)


@dataclass
class Uint32:
    """Unsigned 32-bit integer, big-endian."""

    value: int = 0

    def __len__(self) -> int:
        return 4

    def serialize(self) -> bytes:
        """Serialize to four bytes, big-endian."""
        return struct.pack(">I", self.value)

    @classmethod
    def deserialize(cls, payload: bytes) -> "Uint32":
        """Deserialize from four big-endian bytes."""
        (value,) = struct.unpack(">I", payload[:4])
        return cls(value=value)


@dataclass
class Sint32:
    """Signed 32-bit integer, big-endian."""

    value: int = 0

    def __len__(self) -> int:
        return 4

    def serialize(self) -> bytes:
        """Serialize to four bytes, big-endian."""
        return struct.pack(">i", self.value)

    @classmethod
    def deserialize(cls, payload: bytes) -> "Sint32":
        """Deserialize from four big-endian bytes."""
        (value,) = struct.unpack(">i", payload[:4])
        return cls(value=value)


@dataclass
class Float32:
    """IEEE 754 single-precision float, big-endian."""

    value: float = 0.0

    def __len__(self) -> int:
        return 4

    def serialize(self) -> bytes:
        """Serialize to four bytes, big-endian IEEE 754."""
        return struct.pack(">f", self.value)

    @classmethod
    def deserialize(cls, payload: bytes) -> "Float32":
        """Deserialize from four big-endian IEEE 754 bytes."""
        (value,) = struct.unpack(">f", payload[:4])
        return cls(value=value)


# ---------------------------------------------------------------------------
# Composite: SomeIpPayload
# ---------------------------------------------------------------------------


class SomeIpPayload:
    """Base class for SOME/IP struct payloads.

    Subclasses declare fields as instance attributes, usually in ``__init__``.
    ``serialize()`` joins the ``serialize()`` results of all public attributes in insertion order.
    ``deserialize()`` reads the fields in the same order. It moves through the buffer by the ``__len__`` of each field.

    Attributes with a name that starts with ``_`` are not encoded.
    """

    def __len__(self) -> int:
        """Return the total wire size in bytes."""
        return sum(len(v) for k, v in self.__dict__.items() if not k.startswith("_"))

    def serialize(self) -> bytes:
        """Serialize all public fields in insertion order to bytes."""
        out = b""
        for k, v in self.__dict__.items():
            if not k.startswith("_"):
                out += v.serialize()
        return out

    def deserialize(self, payload: bytes) -> "SomeIpPayload":
        """Deserialize *payload* into the public fields of this object, in order.

        A field with a ``_has_dynamic_size`` attribute (for example ``SomeIpDynamicSizeArray``) gets the
        remaining part of the buffer. The field reads its own length prefix. Then ``__len__`` gives its used length.

        Args:
            payload: Raw bytes that start at the first field of this struct.

        Returns:
            self, so that you can chain calls: ``obj.deserialize(buf).field``.
        """
        pos = 0
        for k, v in self.__dict__.items():
            if k.startswith("_"):
                continue
            if getattr(v, "_has_dynamic_size", False):
                new_v = v.deserialize(payload[pos:])
                setattr(self, k, new_v)
                pos += len(new_v)
            else:
                field_len = len(v)
                new_v = v.deserialize(payload[pos : pos + field_len])
                setattr(self, k, new_v)
                pos += field_len
        return self


# ---------------------------------------------------------------------------
# Arrays
# ---------------------------------------------------------------------------

T = TypeVar("T")


class SomeIpFixedSizeArray(Generic[T]):
    """Fixed-size array of SOME/IP primitive or payload elements of one type.

    Wire format: the elements one after the other, with no length prefix.

    Args:
        class_reference: The element type. It must have a default constructor,
                         ``serialize()``, ``deserialize()``, and ``__len__``.
        size:            Number of elements.
    """

    def __init__(self, class_reference: Type[T], size: int) -> None:
        self.data: List[T] = [class_reference() for _ in range(size)]

    def __len__(self) -> int:
        if not self.data:
            return 0
        return len(self.data) * len(self.data[0])

    def serialize(self) -> bytes:
        """Serialize all elements one after the other, with no length prefix."""
        return b"".join(e.serialize() for e in self.data)

    def deserialize(self, payload: bytes) -> "SomeIpFixedSizeArray":
        """Deserialize the elements from *payload* with the fixed size of each element."""
        if not self.data:
            return self
        elem_len = len(self.data[0])
        for i, elem in enumerate(self.data):
            self.data[i] = elem.deserialize(payload[i * elem_len : (i + 1) * elem_len])
        return self


class SomeIpDynamicSizeArray(Generic[T]):
    """Dynamic-size array of SOME/IP elements of one type.

    Wire format: ``[length-field: N bytes big-endian][element 0][element 1]...``
    The length field has the total number of bytes of all elements, not the number of elements.

    Args:
        class_reference: The element type. It gives the fixed wire size of an element,
                         and it makes new instances when you deserialize.
    """

    _has_dynamic_size = True

    def __init__(self, class_reference: Type[T]) -> None:
        self._class_reference = class_reference
        self._single_element_length: int = len(class_reference())
        self._data: List[T] = []
        self._length_field_length: int = 4

    # -- property: data -------------------------------------------------------

    @property
    def data(self) -> List[T]:
        """List of the elements."""
        return self._data

    @data.setter
    def data(self, value: List[T]) -> None:
        self._data = value

    # -- property: length_field_length ----------------------------------------

    @property
    def length_field_length(self) -> int:
        """Number of bytes of the length prefix (0, 1, 2, or 4)."""
        return self._length_field_length

    @length_field_length.setter
    def length_field_length(self, value: int) -> None:
        if value not in (0, 1, 2, 4):
            raise ValueError("length_field_length must be 0, 1, 2, or 4")
        self._length_field_length = value

    # -- sizing ---------------------------------------------------------------

    def __len__(self) -> int:
        return self._length_field_length + len(self._data) * self._single_element_length

    # -- wire encoding --------------------------------------------------------

    _LF_FMT = {1: ">B", 2: ">H", 4: ">I"}

    def serialize(self) -> bytes:
        """Serialize the length prefix, then the bytes of all elements."""
        body = b"".join(e.serialize() for e in self._data)
        if self._length_field_length == 0:
            return body
        return struct.pack(self._LF_FMT[self._length_field_length], len(body)) + body

    def deserialize(self, payload: bytes) -> "SomeIpDynamicSizeArray":
        """Deserialize from *payload*. Read the length prefix first."""
        self._data = []
        if self._length_field_length == 0:
            data_bytes = payload
        else:
            fmt = self._LF_FMT[self._length_field_length]
            (length,) = struct.unpack_from(fmt, payload)
            data_bytes = payload[self._length_field_length : self._length_field_length + length]

        n = len(data_bytes) // self._single_element_length
        for i in range(n):
            start = i * self._single_element_length
            elem = self._class_reference.deserialize(data_bytes[start : start + self._single_element_length])
            self._data.append(elem)
        return self
