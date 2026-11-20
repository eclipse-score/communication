# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

"""Target-side scratch paths of the SOME/IP send-path benchmark."""

from __future__ import annotations

from dataclasses import dataclass

from perf_itf.shell import checked_root


@dataclass(frozen=True)
class BenchPaths:
    """Target-side scratch paths. All paths come from one writable root (``--target-tmp-dir``).

    One root keeps all paths together. The pcap for the analyzer, each log with a ready marker and the deploy of
    someipd must be on a filesystem that the target can write. That filesystem is different for each target.
    """

    root: str

    @classmethod
    def for_root(cls, root: str) -> BenchPaths:
        """Normalize and validate *root*. All paths below go unquoted into ``rm -rf`` and ``mkdir -p``.

        If you make :class:`BenchPaths` directly, there is no check.
        """
        return cls(checked_root(root, "--target-tmp-dir"))

    def _in_root(self, name: str) -> str:
        return f"{self.root}/{name}"

    @property
    def deploy_mount(self) -> str:
        """The upload directory of the deploy."""
        return self._in_root("someip_benchmark")

    @property
    def deploy_archive(self) -> str:
        return self._in_root("deploy.tar")

    @property
    def sendlog(self) -> str:
        return self._in_root("sendlog.csv")

    @property
    def pcap(self) -> str:
        return self._in_root("sender.pcap")

    @property
    def capture_log(self) -> str:
        return self._in_root("capture.log")

    @property
    def cpu_log(self) -> str:
        return self._in_root("cpu.log")

    @property
    def mem_log(self) -> str:
        return self._in_root("mem.log")

    @property
    def cpu_pidfile(self) -> str:
        return self._in_root("cpu_sampler.pid")

    @property
    def tso_marker(self) -> str:
        """Exists while a TCP run has TSO off on the capture NIC. The next run uses it to set TSO on again."""
        return self._in_root("tso_disabled")

    @property
    def someipd_log(self) -> str:
        return self._in_root("someipd.log")

    @property
    def sender_log(self) -> str:
        return self._in_root("sender.log")

    @property
    def run_logs(self) -> tuple[str, ...]:
        """Target-side process logs that each run copies to the host. The subscriber log is already on the host."""
        return (
            self.someipd_log,
            self.sender_log,
            self.capture_log,
            self.cpu_log,
            self.mem_log,
            self.sendlog,
            self.pcap,
        )

    @property
    def stale_artifacts(self) -> tuple[str, ...]:
        """All files that a run writes into the scratch root, for the clean-up before a run on a device.

        The list includes the deploy archive. Only the end of a successful extraction removes it.
        Thus an upload that stops before that end leaves the full tar file.
        """
        return (self.cpu_pidfile, self.deploy_archive, *self.run_logs)
