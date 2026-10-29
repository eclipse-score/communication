# Building and Running on QNX

LG SOME/IP is cross-compiled for QNX 8 with the QNX SDP 8.0 toolchains that the
`score_communication` workspace provides. The QNX build is part of the QNX
GitHub workflow of the repository.

## Requirements

Building for QNX requires:

- An account on qnx.com with an assigned QNX 8 license.
- The credentials of this account, either in `~/.netrc` (`machine qnx.com`) or
  in the `SCORE_QNX_USER` and `SCORE_QNX_PASSWORD` environment variables. Bazel
  uses them to download the QNX SDP.
- The QNX license under `/opt/score_qnx/license/licenses`. Copy the
  `~/.qnx/license/licenses` directory that the QNX Software Center creates when
  installing the QNX SDP 8.

See the QNX section of the repository [.bazelrc](../../../.bazelrc) for details.

## Build

Run the Bazel commands from the root of the `score_communication` workspace:

```sh
# x86_64 (also available as --config=qnx)
bazel build --config=qnx_x86_64 //score/lg-someip/...

# aarch64
bazel build --config=qnx_arm64 //score/lg-someip/...
```

On QNX, QNX message passing is used for local IPC between applications and the
daemon instead of Unix domain sockets. The binaries are statically linked
against lg-someip and are written below `bazel-bin/score/lg-someip/`.

## Deploy and Run

Copy the binaries and a configuration file to the target, for example to
`/var/lg-someip`, then:

```sh
export LGSOMEIP_CONFIGURATION=/var/lg-someip/etc/lgsomeip_config.json
/var/lg-someip/bin/someip-daemon &
/var/lg-someip/bin/someip-request-1
```

- `someip-daemon` takes the configuration path as its first argument. Every
  application, including the daemon, otherwise reads `LGSOMEIP_CONFIGURATION`.
  The `vsomeip` interface samples read `VSOMEIP_CONFIGURATION` and
  `VSOMEIP_APPLICATION_NAME` instead.
- `unicast` must be an IP address that is assigned to a network interface of
  the host that runs the daemon. Use a different configuration file on each
  host.
- Set `is-provider` to `true` on the host that offers a service and to `false`
  on the host that consumes it.

## Known Limitations

- Some QNX SDP versions do not define `SO_VLANPRIO`. On those platforms the
  VLAN priority is not applied and a warning is logged when a priority is
  configured.
