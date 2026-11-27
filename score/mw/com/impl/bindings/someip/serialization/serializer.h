#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SERIALIZER_H

#include "score/result/result.h"

#include <score/span.hpp>

#include <cstddef>
#include <cstdint>

namespace score::mw::com::impl
{

class ISerializer
{
  public:
    virtual ~ISerializer() = default;

    virtual score::Result<std::size_t> Serialize(const void* sample,
                                                 score::cpp::span<std::uint8_t> buffer) const noexcept = 0;
    virtual score::Result<std::size_t> Deserialize(score::cpp::span<const std::uint8_t> buffer,
                                                   void* sample) const noexcept = 0;
    virtual score::Result<std::size_t> GetMaxSerializedSize() const noexcept = 0;
};

}  // namespace score::mw::com::impl

#endif