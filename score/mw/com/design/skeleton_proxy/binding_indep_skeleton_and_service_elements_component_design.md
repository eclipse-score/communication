### Skeleton and Service Element Creation

In order to perform any service discovery related operations on the skeleton side it is necessary to instantiate the
skeleton by the user. This has no restrictions of any kind. For this purpose, the user would create an instance of a
generated skeleton (`DummySkeleton`).

The concrete (technical) binding to be used is defined within the deployment information, which gets evaluated at
runtime! The transition from the binding independent part to binding dependent part is done via the `pImpl` idiom.
So `SkeletonBase`, which is the base class for the (generated) skeleton, contains a pointer to the binding specific
implementation. All (technical) bindings have to implement `SkeletonBinding`.

During construction of `SkeletonBase` the `pImpl` gets initialized from the information contained in the given
`InstanceIdentifier` (or `InstanceSpecifier`, which gets resolved into an `InstanceIdentifier`). The resolution of the
correct binding and the construction of the correct subclass of `SkeletonBinding` is performed by `SkeletonBindingFactory` 
(TODO: Link to the binding factory component).


However, the generated skeleton is a composite, potentially containing  (depending on its interface description) a number
of different event (or also field) members. For those composite members the same `pImpl` pattern is applied.
So `SkeletonEvent` is the binding independent class template (template arg is the data type of the event), which holds
the `pImpl`, which points to an abstract class `SkeletonEventBinding`, which has to be implemented by each (technical)
binding. The mechanism to initialize the `pImpl` is conceptually the same as within the `SkeletonBase`.

