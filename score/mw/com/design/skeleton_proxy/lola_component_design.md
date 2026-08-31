

In the case of our shared memory implementation (also called `LoLa`), it is necessary to access an underlying, not yet
further specified shared memory region which is specific to each communication instance. A problem can arise if the same
process includes a skeleton and proxy side of the same service instance. In this case we have to ensure that the shared
memory region is not mapped twice into the process space. In order to do that, the
`memory::shared::MemoryResourceRegistry` will take care to check which paths have already been created and will return
or create the necessary resources if necessary.

The details, if and when a shared memory object created by a communication instance gets cleaned up, can be read in the
[detailed design of partial restart functionality](../partial_restart/README.md#partial-restart-specific-extensions-to-skeletonpreparestopoffer)

