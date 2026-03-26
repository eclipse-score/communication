# OffsetPtr problems to solve

## Definitions / background:
- Start check: Checking that the start address of a pointed-to object lies the same shared memory region in which the OffsetPtr was created.
- End checks: Checking that the end address of a pointed-to object lies the same shared memory region in which the OffsetPtr was created.
- Bounds check: Start and end check.

One-past-the-end-iterators:
- The size of memory allocated for a container usually doesn't include the size of a pointed-to object starting at a one-past-the-end iterator address. Therefore, doing an end check on a one-past-the-end iterator may fail.
    - It is legal to create and copy such a pointer. 
    - It's illegal to dereference such a pointer.
    - It's legal to decrement such a pointer and then dereference it.
- The standard library containers that we're using (i.e. std::unordered_map) are creating and copying one-past-the-end pointers.

## Required checks: 

OffsetPtr<PointedType != void>
- Dereferencing a pointer - operator*
    - We need to do a Bounds check.
- Getting raw pointer - get(), operator pointer(), operator->
    - Although we're not dereferencing the pointer, so we are not violating any safety goals by creating the raw pointer, we cannot control the bounds checking after this point (i.e. making sure that bounds checking is done before the raw pointer is dereferenced). Therefore, we need to do the same checks as if we were dereferencing the pointer.
- Copying OffsetPtr
    - We don't _need_ to do any checks here. The copied-to OffsetPtr will do any required bounds checking when it's dereferenced.

OffsetPtr<PointedType == void>
- Dereferencing a pointer - operator*
    - We cannot dereference a void*.
- Getting raw pointer - get(), operator pointer(), operator->
    - We do not know the size of the pointed-to object and therefore can only perform a start check. 
    - It must be ensured that a full bounds check is done before the user dereferences the pointer (after they've cast it to a type). i.e. if we do a start check when getting the pointer, the user just has to do the end check.
- Copying OffsetPtr
    - We don't _need_ to do any checks here. The copied-to OffsetPtr / subsequent user checks before dereferencing will do any required bounds checking.


## Problems to solve:

1. UB in pointer arithmetic when calculating offset and getting absolute pointer from offset.
2. Copying offset pointer should not do end check (will fail for one-past-the-end iterator).
3. Getting a void pointer cannot do end check. Dereferencing retrieved pointer will therefore bypass end check.
4. Copying an OffsetPtr to the stack will no longer perform bounds checking. Therefore, we must be able to do bounds checking when getting / dereferencing a stack OffsetPtr.

## Proposed solution:

1. Cast addresses to integral types (implementation defined: `https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#4`, `https://timsong-cpp.github.io/cppwp/n4659/expr.reinterpret.cast#5`) and then do all arithmetic on integral types to avoid UB.
    - When calculating the offset, cast the OffsetPtr and pointed-to addresses to integral type and subtract integers.
    - When getting raw pointer from offset, cast the OffsetPtr address to integral type and add the offset to it. Cast the resulting address to a pointer.
    - When copying an OffsetPtr, cast the copied-from and copied-to OffsetPtr addresses to integral types. Add the difference between the two integers to the currently stored offset and store in the copied-to OffsetPtr.
2. Copying offset pointer can now calculate the offset in an implementation defined manner (even when copying out of shared memory) so doesn't have to do any bounds checking. Therefore, copying one-past-the-end iterator will do no end check and never fail.
3. OffsetPtr<void> could contain a templated get() (which would be the only one enabled for PointedType=void) which is templated with the real size of the type (which is currently pointed to via a void pointer). It will then do the bounds check using the size of the and return a void* or event the type itself.
    - Limitation of this is that we use OffsetPtr<void> instead of void*. We would have to change ManagedMemoryResource getUsableBaseAddress, getEndAddress etc. to return OffsetPtr<void> instead of void*. We wanted to anyway make these private, so it might not be a big issue.
    - Other option would be to have getUnsafe() and require the caller to manually do the bounds checking.
4. When copying an OffsetPtr to the stack, we must store the MemoryResource identifier to do the bounds checking on dereferencing (since we can't use the address of the OffsetPtr itself to identiy the memory resource, like when the OffsetPtr is in shared memory).

## Misc:
- MemoryResourceRegistry can store uintptr_t instead of void* for memory bounds.
