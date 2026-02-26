# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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
import logging
import subprocess
import time
import re
from queue import Empty
from typing import Optional

from quality.integration_testing.environments.qnx8_qemu.line_reader import LineReader


def try_to_encode(data, encoding='ascii'):
    if isinstance(data, str):
        return data.encode(encoding)
    if isinstance(data, bytes):
        return data
    raise TypeError("could not encode data. must be a str or bytes")


def try_to_decode(data, encoding='ascii'):
    if isinstance(data, bytes):
        data = re.sub(b'\r[^\n]', b'', data)
        return data.decode(encoding, 'replace').rstrip("\n").rstrip("\r")
    if isinstance(data, str):
        return data.rstrip("\n").rstrip("\r")
    raise TypeError("could not decode data. must be a str or bytes")


def try_to_ascii(data):
    return try_to_encode(data, 'ascii')


def try_to_decode_ascii(data):
    return try_to_decode(data, 'ascii')


class Console:

    def __init__(self, name, reader, writer, print_logger=True, logfile=None):
        self.name = name
        self.writer = writer
        self.line_reader = LineReader(
            readline_func=reader,
            name=self.name,
            print_logger=print_logger,
            logfile=logfile,
        )
        self.line_reader.start()

    @property
    def print_logger(self):
        return self.line_reader.print_logger

    @print_logger.setter
    def print_logger(self, value):
        self.line_reader.print_logger = value

    def readline(self, block=False, timeout=None):
        if self.line_reader:
            return self.line_reader.get_line(block, timeout)
        return None

    def write(self, command):
        self.writer(command)

    def run_cmd(self, cmd):
        if cmd is not None:
            if callable(cmd):
                cmd()
            else:
                self.write(cmd)

    def run_sh_cmd_output(self, cmd, timeout=30):
        start_time = time.time()

        cmd_finish = "COMMAND_DONE"

        self.clear_history()
        self.write(f"{cmd}; echo {cmd_finish}=$?")

        output = []
        while True:
            remaining = start_time - time.time() + timeout

            if remaining <= 0:
                raise Exception("Timed out waiting for command to finish")

            try:
                line = self.readline(block=True, timeout=remaining)
            except Empty as empty:
                raise Exception("Timed out waiting for command to finish") from empty

            if cmd_finish in line and cmd in line:
                continue

            if cmd_finish in line:
                spl = line.split(f"{cmd_finish}=")
                output.append(spl[0])
                retcode = int(spl[1])
                return retcode, ('\n'.join(output)).strip()

            output.append(line)

    def run_sh_cmd_async(self, cmd, timeout=30):
        self.clear_history()
        self.write(f"{cmd} &")
        exit_code, output = self.run_sh_cmd_output(f"echo CREATED_PID=$!", timeout=timeout)
        assert exit_code == 0
        return int(re.search(r'CREATED_PID=(\d+)', output).group(1))


    def add_expr_cbk(self, expr, cbk, regex=False):
        self.line_reader.add_expr_cbk(expr, cbk, regex)

    def _expect(self, cmd, msgs, timeout, regex=False, end_func=any, clear_history=True):
        if clear_history:
            self.clear_history()
        self.run_cmd(cmd)
        if isinstance(msgs, str):
            msgs = [msgs]
        if self.line_reader.read_cond(msgs, timeout, regex, end_func):
            return True
        raise Exception(f"Failed expect {end_func.__name__}: {cmd}: {msgs}")

    def expect_any(self, cmd, msgs, timeout, regex=False, clear_history=True):
        return self._expect(cmd, msgs, timeout, regex, any, clear_history)

    def expect_all(self, cmd, msgs, timeout, regex=False, clear_history=True):
        return self._expect(cmd, msgs, timeout, regex, all, clear_history)

    def mark(self, cmd, msgs, timeout, clear_history=True):
        if clear_history:
            self.clear_history()
        self.run_cmd(cmd)
        time_points = []
        for msg in msgs:
            if self.line_reader.read_until(msg, timeout):
                time_points.append((msg, time.time()))
            else:
                time_points.append((msg, None))
        return time_points

    def clear_history(self):
        self.line_reader.clear_history()


class PipeConsole(Console):
    """Handles interaction with a subprocess through stdin and stdout.

    This class provides an interface for interacting with a subprocess through
    its stdin and stdout streams. It allows sending commands to the subprocess
    and reading its output with configurable timeout and logging options.
    """

    def __init__(
            self,
            name: str,
            process: subprocess.Popen,
            timeout: int = 10,
            linefeed: str = "\n",
            logfile: Optional[str] = None,
            print_logger: bool = True,
    ) -> None:
        r"""Initializes the PipeConsole instance.

        Args:
            name: Name of the console.
            process: The subprocess.Popen instance representing the process.
            timeout: Timeout in seconds for reading from stdout. Defaults to 10.
            linefeed: Linefeed character(s) to use when sending commands.
                Defaults to '\n'.
            logfile: Path to the log file for logging output. Defaults to None.
            print_logger: Flag to enable or disable logging to the console.
                Defaults to True.

        Attributes:
            _linefeed: Linefeed character(s) to use when sending commands.
            _process: The subprocess.Popen instance representing the process.
            _logger: Logger instance for logging output.
        """
        self._timeout = timeout
        self._linefeed = linefeed
        self._process = process
        self._logger = logging.getLogger(str(process))

        def reader() -> Optional[str]:
            """Reads a line from the process's stdout with a timeout.

            Continuously checks if the process's stdout is ready for reading
            and reads a line from it. If the read operation times out, it
            returns None. If EOF is detected, it breaks the loop.

            Returns:
                str: The decoded line from stdout.
                None: If EOF is detected or a timeout/exception occurs.
            """
            while True:
                line = self._process.stdout.readline()
                if not line:  # EOF detected
                    break
                if line.endswith(b'\n') or line.endswith(b'# '):
                    return try_to_decode(line)
            self._process.stdout.close()
            return None

        def writer(command: str) -> None:
            """Writes a command to the process's stdin.

            Ensures the command is encoded with the specifiec encoding and
            and appends the specified linefeed character(s) before flushing
            the stdin stream.

            Args:
                command: The command to be sent to the process's stdin.
            """
            if self._process.poll() is None:
                self._process.stdin.write(try_to_encode(command + '\n'))
                self._process.stdin.flush()

        super().__init__(name, reader, writer, print_logger)
