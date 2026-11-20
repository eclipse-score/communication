# Third-Party Licenses and Attributions

LG-owned and original LG SOME/IP code is distributed under the Apache License,
Version 2.0. See [LICENSE](LICENSE) for the full text.

Third-party components retain their original licenses; this repository is
therefore distributed under more than one license.

Portions of this project are derived from, or depend on, third-party software.
Their copyright notices and licenses are reproduced or referenced below.

---

## Bundled Source (redistributed with this repository)

### vsomeip interface and E2E protection library

- **Location:** `third_party/vsomeip_interface/`, `third_party/e2e_protection/`
- **Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)**
- **License:** Mozilla Public License, Version 2.0 (MPL-2.0)
- **License text:** `third_party/vsomeip_interface/COPYING.MPL-2.0`, `third_party/e2e_protection/COPYING.MPL-2.0`

These components originate from the GENIVI/COVESA vsomeip project and remain
under their original MPL-2.0 terms. Files that carry a BMW AG copyright notice
are **not** relicensed by this project.

---

## External Dependencies (not bundled; resolved at build time)

The following libraries are not included in this repository. They must be
provided by the build environment. Their licenses apply to those components
only, and their attributions must accompany any binary distribution that links
them.

| Component | License | Usage | Homepage |
|---|---|---|---|
| RapidJSON | MIT | Runtime — configuration parsing (`src/someip/config/`) | https://github.com/Tencent/rapidjson |
| OpenSSL | Apache License 2.0 | Optional — TLS/DTLS secure connections (`ENABLE_TLS` build) | https://www.openssl.org |
| COVESA DLT (automotive-dlt) | MPL-2.0 | Optional — DLT logging backend (`ENABLE_DLT` build) | https://github.com/COVESA/dlt-daemon |
| GoogleTest | BSD-3-Clause | Test builds only (not shipped in the library or daemon) | https://github.com/google/googletest |

### RapidJSON — MIT License

```
Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.

Licensed under the MIT License (the "License"); you may not use this file except
in compliance with the License. You may obtain a copy of the License at

    http://opensource.org/licenses/MIT

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
```

### GoogleTest — BSD 3-Clause License

Full text: https://github.com/google/googletest/blob/main/LICENSE
