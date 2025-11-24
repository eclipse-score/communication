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
import re
import threading
import time
from collections import defaultdict, deque
from contextlib import nullcontext
from datetime import datetime
from queue import Empty


class LineReader(threading.Thread):
    """
    This class launches a separate thread to read line-by-line
    messages from a specific pipe that blocks, unblocking when
    a new message is ready, or when the pipe is closing,
    returning None. The messages are stored in a queue, and
    can be later retrieved.
    """
    log_locks = {}
    log_queues = {}

    def __init__(self, readline_func, name, print_logger=True, logfile=None):
        super().__init__(name=name)
        self.readline_func = readline_func
        self.name = name
        self.logger = logging.getLogger(name)
        self.print_logger = print_logger
        self._logfile = logfile
        self._log_queue = LineReaderQueue(max_size=400)
        self._expr_cbks = defaultdict(lambda: [])
        if logfile:
            if logfile not in LineReader.log_locks:
                LineReader.log_locks[logfile] = threading.Lock()
                LineReader.log_queues[logfile] = LineReaderQueue(max_size=400)
            self._log_queue = LineReader.log_queues[logfile]

    def run(self):
        with open(self._logfile, encoding="utf-8", mode="a") if self._logfile else nullcontext() as logfile:
            while True:
                try:
                    line = self.readline_func()
                except Exception:
                    line = None
                if line is None:
                    break
                line = line.replace('\x00', '')
                line = line.strip()
                message = ""
                if line:
                    message = f"[{datetime.now()}] [{self.name}] - {line}"
                if self.print_logger:
                    self.logger.info(line)
                if self._logfile:
                    with LineReader.log_locks[self._logfile]:
                        try:
                            logfile.write(f"{message} \n")
                            logfile.flush()
                            self._add_log(message)
                        except Exception as exception:
                            self.logger.error(f"Exception on write: {exception}")
                else:
                    self._add_log(line)

                for expr, regex in self._expr_cbks:
                    for cbk in self._expr_cbks[(expr, regex)]:
                        if self._check_msg(line, expr, regex):
                            cbk()

    def add_expr_cbk(self, expr, cbk, regex=False):
        self._expr_cbks[(expr, regex)].append(cbk)

    def read_cond(self, exprs, timeout=90, regex=False, end_func=any):
        start = time.time()
        checks = [False] * len(exprs)
        while True:
            time_remaining = start - time.time() + timeout
            if time_remaining <= 0:
                break
            try:
                line = self.get_line(block=True, timeout=time_remaining)
            except Empty:
                break
            for i, expr in enumerate(exprs):
                if self._check_msg(line, expr, regex):
                    checks[i] = True
            if end_func(checks):
                return True
        return False

    def clear_history(self):
        self._log_queue.clear()

    def read_until(self, expr, timeout=90, regex=False):
        assert isinstance(expr, str)
        return self.read_cond([expr], timeout, regex, any)

    def read_until_one_of(self, exprs, timeout=90, regex=False):
        return self.read_cond(exprs, timeout, regex, any)

    def read_until_all(self, exprs, timeout=90, regex=False):
        return self.read_cond(exprs, timeout, regex, all)

    def read_until_expr(self, expr, timeout=90):
        return self.read_until(expr, timeout, regex=True)

    def read_until_one_of_expr(self, exprs, timeout=90):
        return self.read_until_one_of(exprs, timeout, regex=True)

    def read_until_all_expr(self, exprs, timeout=90):
        return self.read_until_all(exprs, timeout, regex=True)

    def get_line(self, block=False, timeout=None):
        return self._log_queue.get(block=block, timeout=timeout)

    def _add_log(self, log):
        self._log_queue.put(log)

    @staticmethod
    def _check_msg(msg, expr, regex=False):
        return (regex and re.search(expr, msg)) or (not regex and expr in msg)


class LineReaderQueue:
    """
    Thread-safe implementation of a queue.
    When the queue is full, older items
    are removed.
    """

    def __init__(self, max_size=0):
        self.queue = deque()
        self.max_size = max_size
        self.mutex = threading.Lock()
        self.not_empty = threading.Condition(self.mutex)

    def put(self, item):
        with self.mutex:
            if self.max_size > 0 and len(self.queue) >= self.max_size:
                self.queue.popleft()
            self.queue.append(item)
            self.not_empty.notify()

    def get(self, block=True, timeout=None):
        with self.not_empty:
            if not block:
                if len(self.queue) == 0:
                    raise Empty
            elif timeout is None:
                while len(self.queue) == 0:
                    self.not_empty.wait()
            elif timeout < 0:
                raise ValueError("'timeout' must be a non-negative number")
            else:
                endtime = time.time() + timeout
                while len(self.queue) == 0:
                    remaining = endtime - time.time()
                    if remaining <= 0.0:
                        raise Empty
                    self.not_empty.wait(remaining)
            item = self.queue.popleft()
            return item

    def clear(self):
        with self.mutex:
            self.queue.clear()
