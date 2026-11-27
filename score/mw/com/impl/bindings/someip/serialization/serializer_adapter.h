#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_ADAPTER_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_ADAPTER_H

#include "score/mw/com/impl/bindings/someip/serialization/serializer.h"
#include "score/mw/com/impl/bindings/someip/serialization/strategy/someip_strategy.h"

#include "score/serialization/serializer.h"

namespace score::mw::com::impl::someip
{

template <typename DataType>
class SerializerAdapter final : public ISerializer
{
  public:
    explicit SerializerAdapter(const score::serialization::SomeIpLayout& layout) noexcept
        : layout_{layout}, serializer_{score::serialization::SomeIpStrategy{layout}}
    {
    }

    score::Result<std::size_t> Serialize(const void* sample,
                                         score::cpp::span<std::uint8_t> buffer) const noexcept override
    {
        return serializer_.Serialize(*static_cast<const DataType*>(sample), buffer);
    }

    score::Result<std::size_t> Deserialize(score::cpp::span<const std::uint8_t> buffer,
                                           void* sample) const noexcept override
    {
        return serializer_.DeSerializeCompleteData(buffer, *static_cast<DataType*>(sample));
    }

    score::Result<std::size_t> GetMaxSerializedSize() const noexcept override
    {
        return score::serialization::GetMaxSize<DataType>(layout_);
    }

  private:
    score::serialization::SomeIpLayout layout_;
    score::serialization::Serializer<score::serialization::SomeIpStrategy> serializer_;
};

}  // namespace score::mw::com::impl::someip

#endif
