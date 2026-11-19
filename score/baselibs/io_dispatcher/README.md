# io_dispatcher

A reactor: register a callback per file descriptor, block in `HandleEvents()` until one is ready, and run the ready callbacks on the calling thread.
`include/score/io_dispatcher/io_dispatcher.h` holds the contract, and `MakeIoDispatcher()` in `io_dispatcher_factory.h` builds the backend for the target platform.

lg-someip's `Multiplexer` runs on it.

## Backends

The Linux backend waits in `poll()` and wakes through a self-pipe.
It holds at most 8191 sources, since the slot index and a per-slot sequence number share the 64-bit handle.

The QNX backend arms `ionotify()` per descriptor and waits for pulses on a private channel.
A notification pulse carries a 20-bit descriptor and an 8-bit registration generation, so `Register()` refuses a descriptor above 1048575.
The backend drops a pulse whose generation differs from the registration's, so a pulse that a closed socket queued never wakes the next socket on that descriptor number.
`IoDispatcherImpl::RegisterPulse()` additionally routes application pulses sent to `GetConnectionId()` to a callback per pulse code.

## Handles

Every handle carries the issuing dispatcher's `DispatcherTag` in bits 48..50.
`DispatcherTagOf()` is the only decoding a caller may rely on; the remaining bits are backend specific.
