- Binding independent and binding dependent classes (e.g. impl::ProxyBase,
lola::Proxy etc.) must be separate units. We have some functionality which is
implemented in the binding independent layer. If Proxy, ProxyEvent etc. (i.e.
one unit is a lola Proxy which contains binding independent level / binding
dependent level). were single units (both layers) then we would have to have the
design for certain parts duplicated for each binding. E.g. If service discovery
is binding independent, we would have to have diagrams showing the interaction
between service discovery in both the lola and someip proxies.
   - We also have bindings i.e. explicit interfaces for the bindings, so makes
   sense that it's a designed api (more so if the sub teams working on lola and
   someip are different)
   - Since components aggregate the interfaces of components / units below them,
   maybe proxy could be a component which contains binding indep proxy,
   lola::proxy and someip::proxy)?

- Outer Proxy, GenericProxy etc boxes are created so that we can specify the
public interface of the Proxy (e.g.) component which are user would use in
their own architectural diagram of their application.

- Should have a unit "Shm structs". This contains ServiceDataControl/Storage,
EventDataControl/Storage, Provider/ConsumerEventDataControlLocalView. This will
interact with lola::Proxy and lola::Skeleton (and service elements). It would
allow us to mock these for basic unit tests and only actually create shared
memory in integration tests.
    - There's enough complexity in lock free slot algos, local views, dynamic 
    event registration etc to warrant a detailed design.
