# Building and Running on QNX

LG SOME/IP is cross-compiled for QNX with the QNX SDP toolchain. The steps
below were validated with QNX SDP 8.0 (`aarch64le`). QNX builds are not part of
the GitHub CI.

## Requirements

- QNX SDP with a valid license, installed on a Linux host
- CMake >= 3.18
- [RapidJSON](https://github.com/Tencent/rapidjson) headers (set
  `RAPIDJSON_INCLUDE_DIR` for an offline build)

## Toolchain File

Create a CMake toolchain file, for example `qnx-aarch64le.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   "$ENV{QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "$ENV{QNX_HOST}/usr/bin/qcc")
set(CMAKE_C_COMPILER_TARGET   gcc_ntoaarch64le)
set(CMAKE_CXX_COMPILER_TARGET gcc_ntoaarch64le)

set(CMAKE_AR     "$ENV{QNX_HOST}/usr/bin/ntoaarch64-ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB "$ENV{QNX_HOST}/usr/bin/ntoaarch64-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP  "$ENV{QNX_HOST}/usr/bin/ntoaarch64-strip"  CACHE FILEPATH "")
```

## Build

```sh
source <qnx800>/qnxsdp-env.sh      # sets QNX_HOST and QNX_TARGET
cmake -S . -B build-qnx -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/qnx-aarch64le.cmake \
  -DCMAKE_INSTALL_PREFIX=$PWD/install-qnx \
  -DENABLE_TLS=OFF -DENABLE_DLT=OFF -DENABLE_TEST_BUILD=OFF
cmake --build build-qnx --parallel
cmake --install build-qnx
```

On QNX, `ENABLE_QNX_MESSAGE_PASSING` is enabled by default, so local IPC
between applications and the daemon uses QNX message passing instead of Unix
domain sockets.

If `qcc` cannot find its license, point `QNX_CONFIGURATION_EXCLUSIVE` at the
QNX license directory before running CMake.

## Deploy and Run

Copy the install tree to the target, for example to `/var/lg-someip`, then:

```sh
export LD_LIBRARY_PATH=/var/lg-someip/lib
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
