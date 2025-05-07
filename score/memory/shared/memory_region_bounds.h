#ifndef BASELIBS_SCORE_MEMORY_SHARED_MEMORY_REGION_BOUNDS_H
#define BASELIBS_SCORE_MEMORY_SHARED_MEMORY_REGION_BOUNDS_H

#include <cstdint>

namespace score::memory::shared
{

/// \brief Class which stores an optional start / end address of a memory region.
///
/// It uses sentinel values to represent that it is empty i.e. that it doesn't store memory bounds. We do this instead
/// of using std::optional to be as performant as possible. The addresses are stored as std::uintptr_t and can be
/// coverted back to pointers using CastIntegerToPointer in pointer_arithmetic_util.
class MemoryRegionBounds
{
  public:
    MemoryRegionBounds() noexcept;
    MemoryRegionBounds(const std::uintptr_t start_address, const std::uintptr_t end_address) noexcept;

    void Set(const std::uintptr_t start_address, const std::uintptr_t end_address) noexcept;
    void Reset() noexcept;

    bool has_value() const noexcept;
    std::uintptr_t GetStartAddress() const noexcept;
    std::uintptr_t GetEndAddress() const noexcept;

  private:
    std::uintptr_t start_address_;
    std::uintptr_t end_address_;
};

bool operator==(const MemoryRegionBounds& lhs, const MemoryRegionBounds& rhs) noexcept;
bool operator!=(const MemoryRegionBounds& lhs, const MemoryRegionBounds& rhs) noexcept;

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_MEMORY_REGION_BOUNDS_H
