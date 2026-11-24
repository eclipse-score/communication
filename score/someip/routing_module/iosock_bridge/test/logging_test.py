"""Verify C and C++ io-sock logs reach slog2."""

import time


def test_module_logs_to_slog(target):
    exit_code, output = target.execute("mount -T io-sock /opt/someip_routing/bin/mods-someip-routing.so")
    assert exit_code == 0, output.decode(errors="replace")

    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        _, output = target.execute("slog2info")
        if b"Routing module started with LoLa configuration" in output and b"kernel thread started" in output:
            return
        time.sleep(0.1)
    raise AssertionError(output.decode(errors="replace"))
