# Offset Pointer

## Overview 

When mapping shared memory into different processes, the shared memory block will be mapped to a different
virtual address space in each process. While it is theoretically possible to enforce that the shared memory block is
always mapped at the same base address in each process, this is rather impractical, since it cannot be ensured that any
given memory address is still free for use in each process.

This means that using pointers or using complex data structures that rely on pointers in shared memory is non-trivial.
A pointer created in shared memory in one process pointing to an address in the same shared memory region will 
not be valid in another process. `boost::interprocess` solves this problem with introducing a so-called
[OffsetPtr](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/interprocess/offset_ptr.html). The C++-Standard
names such pointers also `fancy pointer`.

The idea of an `OffsetPtr` is that instead of storing an address of the pointed-to object (like a normal pointer), it 
stores the offset between the address of the pointed-to object and the address of the `OffsetPtr` itself. This offset 
is the same in all processes, thus, a valid pointer can be calculated as the sum of the base address of the `OffsetPtr` and 
the offset that it stores.

The available public member methods are taken over from the `boost::interprocess::offset_ptr` implementation. In order to
reuse this pointer also with stl-based containers it shall implement the requirements stated by
[std::pointer_traits](https://en.cppreference.com/w/cpp/memory/pointer_traits).

### Bounds Checking OffsetPtr
For safety reasons, it is important that when accessing the memory pointed to by an `OffsetPtr` (either by dereferencing the `OffsetPtr` or 
getting a raw pointer from the `OffsetPtr` and dereferencing that), the *entire* pointed-to object must lie inside the original memory region in which 
the `OffsetPtr` was created (See [this](../../../../docs/features/ipc/lola/ipnext_README.md#shared-memory-handling) for an explanation of why
bounds checking must be done).

From a safety perspective, the point of bounds checking is to prevent a lower safety rated process from interfering with the memory of 
a higher safety rated process. Currently, this is only an issue when dealing with shared memory. However, in the future, we may have 
other memory resources which also require bounds checking. e.g. memory pools in which we want to make sure that the `OffsetPtr` is 
not pointing to an address outside that pool. Therefore, we have a generic interface for bounds checking that doesn’t depend on the 
type of memory.

Since we may have multiple memory resources which should be bounds checked, the `MemoryResourceRegistry` provides the public interface 
for these checks. An `OffsetPtr` does not know in which region / type of memory it has been allocated, so it is up to the `MemoryResourceRegistry` 
to determine the relevant memory resource and memory bounds, if there are any, associated with a given `OffsetPtr`. This also means that bounds 
checking has to be attempted every time an `OffsetPtr` is dereferenced, even if the `OffsetPtr` is in a type of memory that doesn’t need to be 
bounds checked. Each class deriving from `ManagedMemoryResource` (e.g. `SharedMemoryResource`) can decide whether the memory that it is managing 
should be bounds checked by an `OffsetPtr`. It does this by implementing the function `ManagedMemoryResource::IsOffsetPtrBoundsCheckBypassingEnabled()`. 

[Bounds checking](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/ddad_score/memory/design/shared_memory/bounds_checking.uxf) contains a 
minimalistic UML diagram of the bounds checking.

#### Bounds Checking Performance - Memory Bounds Lookup
Our simple integration tests and feedback from customers revealed, that the bounds checking functionality will be hit
very frequently! In our 1st straight forward implementation approach, the MemoryResourceRegistry::GetBoundsFromAddress()
function acquired a reader-lock as we had to care for consistency between readers asking for bounds and writers, which
update the current bounds by inserting/removing resources. But this solution based on a reader-writer lock turned out to
be a big performance penalty.

Our current solution to access the bounds within the `MemoryResourceRegistry` concurrently between readers and
writers is the following:
* there can be only one writer active at a time. So writers get "serialized" by a "normal" mutex. I.e. all writer activity
  to the bounds are routed through one of the following APIs, which already care for writer serialization:
  `insert_resource()`, `remove_resource()`, `clear()`
* there can be an arbitrary number of readers active, which do a bounds-lookup (this happens during OffsetPtr deref)
* the algo to synchronize the access between the single writer and the multiple readers of the bounds is a lock-free
  algo based on versioning and is detailed in the next subchapter.

So as we have lock-free access to the bounds for our readers, the footprint/runtime during the (high frequency bounds-
checking is very low, which also some benchmarks revealed (see [here](../../shared/test/performance).

##### Lock-Free bounds-check algorithm
The known bounds (aka known regions) are stored in a map (`std::map<const void*, const void*>`) containing the start
address of the region as key and its end address as value. For our lock-free algo, we are maintaining N versions of
this known regions/map and an indicator, which of the N versions is the current/most recent one.

## OffsetPtr Implementation

Points to be considered in implementation can be seen in [Problems to solve](./offset_ptr_problems.md#problems-to-solve).

### Bounds checking - OffsetPtr in shared memory

When an `OffsetPtr` is in a shared memory region, we can perform bounds checks by getting the memory bounds of that region from the 
MemoryResourceRegistry using the address of the OffsetPtr (via `MemoryResourceRegistry::GetBoundsFromAddress`). We then check that
the start address and end address of the pointed-to object lie within the retrieved memory bounds. We also check that the entire
`OffsetPtr` fits within the shared memory region.

### Bounds checking - OffsetPtr on stack

If the `OffsetPtr` is copied out of the memory region in which it was originally created, we still need to perform bounds checks before dereferencing /
getting a raw pointrer from the `OffsetPtr`. Therefore, when copying an `OffsetPtr` from shared memory to the stack, we get the `MemoryResourceIdentifier` 
of the memory resource from the `MemoryResourceRegistry` and store it within the `OffsetPtr`. When dereferencing / getting a raw pointer from an `OffsetPtr` 
on the stack, we can get the memory bounds of the `OffsetPtr`'s memory region with `MemoryResourceRegistry::GetBoundsFromIdentifier`. We can use 
these bounds to check that the pointed-to object is still within that memory region.

When the `OffsetPtr` is copied back into shared memory, the `MemoryResourceIdentifier` is no longer used, since it can be corrupted by another process,
so we have to again use `MemoryResourceRegistry::GetBoundsFromAddress` to look up memory bounds for bounds checking. If the `OffsetPtr` is copied back
to the stack, then the `MemoryResourceIdentifier` will be looked up again.

### Dereferencing / Getting OffsetPtr\<void\>

An `OffsetPtr` can be templated with `void`. This can be useful for applications in which type-erasure of the pointed-to type is required. However, this
means that the `OffsetPtr` does not know the size of the pointed-to object, which is required for checking that the start **and** end address of the pointed-to
object lies within the correct memory region. Therefore, we provide two additional `get()` overloads when the pointed-to type is void to allow the user to 
provide the size information used to check that the end address of the pointed-to object also lies within the correct memory region:

* `get<ExplicitPointedType>`: This allows the caller to provide the actual PointedType as a template argument. 
* `get(explicit_pointed_type_size)`: This allows the caller to provide the size of the PointedType as a function argument. This is useful if the size of the 
pointed-to object is not known at compile time (and hence cannot be derived from a type), e.g. if we have an OffsetPtr pointing to a type erased array of 
dynamic size.

### Copying OffsetPtr

As outlined in [One-past-the-end-iterators](./offset_ptr_problems.md#definitions--background), doing a bounds check on a one-past-the-end iterator may 
fail if the container lies at the end of the memory region. However, we want to support the ability to copy a one-past-the-end iterator. Therefore, we 
have to make sure that copying an `Offsetptr` does not perform bounds checking (even when copying out of shared memory). Since bounds checking only 
needs to be done before getting a raw pointer from the `OffsetPtr` or dereferencing it (which can also be done 
[if the `OffsetPtr` has been copied to the stack](#dereferencing--getting-offsetptr-on-the-stack)), we can simply avoid doing any bounds checks when 
copying without violating any safety goals.

### Bounds check "race conditions"

Since an OffsetPtr residing in shared memory could be corrupted *during* bounds checking, we must ensure that the offset value (or any other value which resides in 
shared memory such as a `MemoryResourceIdentifier`) is first copied to the stack where it cannot be corrupted by another process. This copy should be used for bounds 
checking and once checked, it should be used for dereferencing, getting a raw pointer etc.

### Pointer Arithmetic Considerations

In the implementation of an `OffsetPtr` as described above, we need to perform pointer arithmetic in two places:

1. When constructing or copying an `OffsetPtr`, we need to subtract the address of the `OffsetPtr` itself from the address
of the pointed-to object.
2. When dereferencing an `OffsetPtr`, we need to add the calculated offset to the address of the `OffsetPtr`.

In (1.), subtracting two pointers which do not point to elements of the same array is undefined behaviour according 
to the [standard](https://timsong-cpp.github.io/cppwp/n4659/expr.add#5). In (2.), if adding an integral type to a pointer results in an
address which does not point to an element of the same array, then this is also undefined behaviour according to the 
[standard](https://timsong-cpp.github.io/cppwp/n4659/expr.add#4). To deal with these issues, we first cast the address to an integral type, 
and then do the addition / subtraction on the integral types instead of pointers. We can then cast the integral type back to a pointer, if required. 
The conversion of a pointer to an integral type and an integral type to a pointer are implementation defined: https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#4 
and https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#5, respectively. In this way, all "pointer arithmetic" is
now actually integer arithmetic which is implementation defined. We rely on having sufficient tests to ensure that the implementation
behaves as we expect.

## DynamicArray Considerations

### Bounds checking iterators / element access

LoLa uses [DynamicArrays](../../../containers/dynamic_array.h) for its [ServiceDataStorage](../../../../mw/com/impl/bindings/lola/service_data_storage.h) 
and [ServiceDataControl](../../../../mw/com/impl/bindings/lola/service_data_control.h). A `DynamicArray` is a fixed-size array data structure whose size can be 
dynamically set at construction. Since these both reside in shared memory, the underlying pointer type used by `DynamicArray` must be an `OffsetPtr`. The 
`DynamicArray` is therefore susceptible to similar issues of memory corruption as an `OffsetPtr`.

For example, if the `OffsetPtr` to the underlying array is corrupted, then it may point to an address outside the correct memory region or to an address
that begins within the memory region, but the end address of the array (i.e. the start address + the array size) would reside outside the memory region.

When accessing any elements via `at()` or `operator[]`, we must check that the element lies in the correct memory region. This is automatically
done since we use an `OffsetPtr` to point to the array, so dereferencing an element will already perform bounds checking. However, when getting any iterators or 
pointers from the `DynamicArray`, we must first check that the entire underlying array lies in the correct memory region. We can do this by performing an 
`OffsetPtr` bounds check on the first and last elements of the array. Since the array is contiguous, if the first and last elements are within the region, 
then all elements are. We do the check on the first **and** last elements since the iterators return raw pointers which can be incremented / decremented to
dereference any element of the array.

### One-past-the-end-iterator

As outlined in [One-past-the-end-iterators](./offset_ptr_problems.md#definitions--background), doing a bounds check on a one-past-the-end iterator may fail 
if the container lies at the end of the memory region. Since the `DynamicArray` uses raw pointers as iterators, it needs to get a raw pointer from 
the one-past-the-end `OffsetPtr` (e.g. in `end()`) which does bounds checking which may fail. Therefore, we provide an additional `get()` overload called 
`GetWithoutBoundsCheck()` which the `DynamicArray` can use **only** for getting the raw pointer from the one-past-the-end `OffsetPtr`. To prevent the user
from decrementing this iterator and dereferencing it without any bounds checks, the `DynamicArray` manually does bounds checking on the start
and end elements as described [above](#bounds-checking-iterators--element-access).
