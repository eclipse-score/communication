#!/usr/bin/env python3

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

import http.cookiejar
import json
import netrc
import os
import sys
import urllib.parse
import urllib.request


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


if __name__ == "__main__":
    data = json.load(sys.stdin)

    if "qnx.com" not in data["uri"]:
        eprint("Unsupported domain")
        sys.exit(1)

    if "SCORE_QNX_USER" in os.environ and "SCORE_QNX_PASSWORD" in os.environ:
        login = os.environ["SCORE_QNX_USER"]
        password = os.environ["SCORE_QNX_PASSWORD"]
    else:
        try:
            nrc = netrc.netrc()
            auth = nrc.authenticators("qnx.com")
            if auth:
                login, _, password = auth
            else:
                raise Exception("No credential found for QNX")
        except Exception as excp:
            eprint(excp)
            eprint("Failed getting credentials from .netrc")
            sys.exit(1)

    data = urllib.parse.urlencode(
        {"userlogin": login, "password": password, "UseCookie": "1"}
    )
    data = data.encode("ascii")

    cookie_jar = http.cookiejar.CookieJar()
    cookie_processor = urllib.request.HTTPCookieProcessor(cookie_jar)
    opener = urllib.request.build_opener(cookie_processor)
    urllib.request.install_opener(opener)

    r = urllib.request.urlopen("https://www.qnx.com/account/login.html", data)
    if r.status != 200:
        eprint("Failed to login to QNX")
        sys.exit(1)

    cookies = {c.name: c.value for c in list(cookie_jar)}
    if not "myQNX" in cookies:
        eprint("Failed to get myQNX cookie from login page")
        sys.exit(1)

    myQNX = cookies["myQNX"]
    print(
        json.dumps(
            {
                "headers": {
                    "Cookie": [f"myQNX={myQNX}"],
                }
            }
        )
    )
