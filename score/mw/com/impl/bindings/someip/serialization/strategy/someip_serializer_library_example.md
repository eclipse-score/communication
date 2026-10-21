# SOME/IP serializer library via `SomeIpStrategy` (example)

How a generated **mw::com serializer `.so`** can be built on the `lib/serialization`
framework (`Serializer<SomeIpStrategy>`) so the **strategy** drives the bytes instead
of hand-written per-datatype encode/decode. The framework walks the struct fields; the
library only supplies the strategy and a per-datatype layout.

Prerequisite: `lib/serialization` (framework, `SomeIpStrategy`, `GetMaxSize`) is
open-sourced to baselibs.

## Constraints that shape the design

- **The `SampleType` is the user's type** (shared by producer, consumer, mw::com).
  It must stay plain C++ — you cannot decorate its fields with wire-tag types
  (`BigEndian<T>`, `Aligned<N,T>`). Wire properties live in the *serializer*, not
  in the datatype.
- **The hash cannot be a struct member.** A SampleType may be a `using` alias or a
  scalar (`using Temperature = std::uint32_t;`), which can't hold `kHashId`. The
  hash is associated externally (trait or symbol name).

## (a) Per-datatype serializer object, looked up by hash

Each datatype gets a concrete **serializer object** implementing a small,
hash-agnostic interface. The object does **not** store its hash; the hash is only the
**key** of the lookup that returns the object.

```cpp
// mw::com-facing interface (hand-written once). The object is hash-agnostic.
class ISerializer {
 public:
  virtual ~ISerializer() = default;
  virtual Result<std::size_t> Serialize(const void* in, score::cpp::span<std::uint8_t> out) const = 0;
  virtual Result<void>        Deserialize(score::cpp::span<const std::uint8_t> in, void* out) const = 0;
  virtual std::size_t         GetMaxSerializedSize() const = 0;
};
```

The generator emits, per datatype, one concrete implementation (its body drives the
strategy) and one lookup entry keyed by the datatype's hash. The hash is the key only.

```cpp
// generated, from Franca + deployment (Fibex/ARXML)
class Serializer9F3C final : public ISerializer {
 public:
  Result<std::size_t> Serialize(const void* in, score::cpp::span<std::uint8_t> out) const override;
  Result<void>        Deserialize(score::cpp::span<const std::uint8_t> in, void* out) const override;
  std::size_t         GetMaxSerializedSize() const override;
};

// lookup: the hash is the KEY; the object carries no hash.
const ISerializer* FindSerializer(std::uint64_t hash_id) {
  switch (hash_id) {
    case 0x9F3C'0000'0000'0001ULL: { static const Serializer9F3C s; return &s; }
    // ... one case per datatype ...
    default: return nullptr;
  }
}
```

## (b) The body uses `Serializer<SomeIpStrategy>` + a generated layout descriptor

Per-element byte order/alignment come from a **generated descriptor** (read from the
deployment), handed to the strategy as the fields are walked (via `MetaInformation`) —
not from the field types. For the common case (one byte order + one alignment for the
whole datatype) the descriptor is just a few constants.

```cpp
Result<std::size_t> Serializer9F3C::Serialize(const void* in, score::cpp::span<std::uint8_t> out) const {
    static constexpr SomeIpLayout kLayout = { /* per-field byte order + alignment */ };
    Serializer<SomeIpStrategy> s{SomeIpStrategy{kLayout}};
    return s.Serialize(*static_cast<const Temperature*>(in), out);
}
std::size_t Serializer9F3C::GetMaxSerializedSize() const { return GetMaxSize<Temperature>(kLayout); }
```

The datatype is annotated once so the framework can walk its fields:

```cpp
SCORE_STRUCT_VISITABLE(Temperature, celsius, raw, recent);   // plain user struct, unchanged fields
```

### The layout descriptor

```cpp
enum class ByteOrder : std::uint8_t { kBigEndian, kLittleEndian };

struct ElementLayout {
    ByteOrder    byte_order{ByteOrder::kBigEndian};
    std::uint8_t alignment_bits{8U};       // 8/16/32/64; 8 = packed (no padding)
    std::uint8_t length_field_bytes{4U};   // width of the length prefix for dynamic members (string/vector)
};

struct SomeIpLayout {
    ElementLayout        default_{};        // applies to every member
    const ElementLayout* per_field{nullptr}; // overrides by field order; nullptr = uniform
    std::size_t          per_field_count{0U};
};
```

Generated — uniform datatype (the common case) is just the default:

```cpp
static constexpr SomeIpLayout kLayout9F3C = { { ByteOrder::kBigEndian, 8U, 4U }, nullptr, 0U };
```

Per-element datatype uses a field-indexed table:

```cpp
static constexpr ElementLayout kFields9F3C[] = {
    { ByteOrder::kBigEndian,     8U, 4U },   // celsius
    { ByteOrder::kLittleEndian,  8U, 4U },   // raw
    { ByteOrder::kBigEndian,    32U, 2U },   // recent (32-bit aligned, dynamic, 2-byte length prefix)
};
static constexpr SomeIpLayout kLayout9F3C = { { ByteOrder::kBigEndian, 8U, 4U }, kFields9F3C, 3U };
```

The strategy resolves each field by its index (from `MetaInformation`) and applies it:

```cpp
ElementLayout For(std::size_t i) const {
    return ((layout_.per_field != nullptr) && (i < layout_.per_field_count))
               ? layout_.per_field[i] : layout_.default_;
}
// scalar path:
const ElementLayout e = For(visitor.GetMetaInformation().field_index_);
EmitAlignmentPadding(it, e.alignment_bits);            // uses it.Offset()
WriteScalar(it, value, NeedReverseTo(e.byte_order));
```

So the user struct stays plain; wire config lives only in `kLayout`. The one framework
addition this needs is a **field index** in `MetaInformation` (today it carries only
first/last); `Offset()` covers alignment.


## What is (and isn't) saved

- **Saved:** no hand-written byte-level encode/decode per datatype — the strategy
  produces the bytes; one encode/decode/size path to test; structurally identical
  datatypes dedup via the hash.
- **Still generated per datatype:** a tiny entry point + a layout descriptor + the
  `SCORE_STRUCT_VISITABLE` annotation. So it is a large reduction, not "only the
  annotation."

## Byte order and alignment (where they live)

- Both are **per-element deployment** properties, carried in the generated **layout
  descriptor** and applied by the strategy via `MetaInformation` — never in the user
  type, never on the wire as metadata.
- **Byte order** is local to a field.
- **Alignment** padding depends on the **runtime write offset from payload start**, so
  the strategy reads the cursor position (`SerializeIterator::Offset()`, the one small
  framework hook) to emit `pad = (align - offset % align) % align`. 8-bit (packed)
  alignment needs no padding.
- Padding bytes occupy wire space but are implicit; the receiver skips them using the
  agreed alignment from its own descriptor.
