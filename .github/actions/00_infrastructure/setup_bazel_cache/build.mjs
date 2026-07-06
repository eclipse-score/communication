// SPDX-License-Identifier: Apache-2.0
import * as esbuild from "esbuild";

// When bundling ESM packages (e.g. @actions/cache -> @azure/storage-common) into
// a CJS bundle, esbuild replaces `import.meta.url` with an empty object property
// that is never initialised at runtime.  Provide the correct file:// URL via a
// banner variable so that createRequire / fileURLToPath calls inside those
// bundled modules receive a valid value.
const sharedOptions = {
  bundle: true,
  platform: "node",
  format: "cjs",
  banner: {
    js: 'var __importMetaUrl__ = require("url").pathToFileURL(__filename).href;',
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
