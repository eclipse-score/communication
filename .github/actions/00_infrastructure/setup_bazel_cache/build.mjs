/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

import * as esbuild from "esbuild";

// When bundling ESM packages (e.g. @actions/cache -> @azure/storage-common) into
// a CJS bundle, esbuild replaces `import.meta.url` with an empty object property
// that is never initialised at runtime.  Provide the correct file:// URL via a
// banner variable so that createRequire / fileURLToPath calls inside those
// bundled modules receive a valid value.
const licenseHeader = `/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/`;

const sharedOptions = {
  bundle: true,
  platform: "node",
  format: "cjs",
  banner: {
    js: `${licenseHeader}\nvar __importMetaUrl__ = require("url").pathToFileURL(__filename).href;`,
  },
  define: {
    "import.meta.url": "__importMetaUrl__",
  },
};

await esbuild.build({
  ...sharedOptions,
  entryPoints: ["src/main.js"],
  outfile: "dist/main/index.js",
});

await esbuild.build({
  ...sharedOptions,
  entryPoints: ["src/post.js"],
  outfile: "dist/post/index.js",
});
