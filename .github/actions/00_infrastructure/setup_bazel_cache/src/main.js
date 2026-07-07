// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

import * as core from "@actions/core";
import * as cache from "@actions/cache";
import path from "path";
import os from "os";
import fs from "fs";

async function run() {
  try {
    const mode = core.getInput("cache-mode");
    const diskCacheName = core.getInput("disk-cache-name");
    const token = core.getInput("github-token");

    // Save inputs to state for the post step
    core.saveState("cache-mode", mode);
    core.saveState("disk-cache-name", diskCacheName);
    core.saveState("github-token", token);

    if (mode === "disabled") {
      core.info("Cache mode: disabled — skipping restore and save registration.");
      return;
    }

    const home = os.homedir();
    const repoCacheDir = path.join(home, ".cache", "bazel", "repository_cache");
    const diskCacheDir = path.join(home, ".cache", "bazel", "disk_cache");

    // Create cache directories
    fs.mkdirSync(repoCacheDir, { recursive: true });
    fs.mkdirSync(diskCacheDir, { recursive: true });

    // Restore repository cache
    if (mode === "read-only" || mode === "update-disk" || mode === "recreate-update") {
      core.info("Restoring repository cache...");
      const repoKey = await cache.restoreCache(
        [repoCacheDir],
        "repo-cache-impossible-exact-match",
        ["repo-cache-"]
      );
      if (repoKey) {
        core.info(`Repository cache restored from key: ${repoKey}`);
      } else {
        core.info("No repository cache found.");
      }
    }

    // Restore disk cache
    if (diskCacheName && (mode === "read-only" || mode === "update-disk")) {
      const runId = process.env.GITHUB_RUN_ID || "0";
      core.info(`Restoring disk cache '${diskCacheName}'...`);
      const diskKey = await cache.restoreCache(
        [diskCacheDir],
        `disk-cache-${diskCacheName}-${runId}`,
        [`disk-cache-${diskCacheName}-`]
      );
      if (diskKey) {
        core.info(`Disk cache restored from key: ${diskKey}`);
      } else {
        core.info("No disk cache found.");
      }
    }

    // Configure Bazel cache paths in ~/.bazelrc
    const bazelrc = path.join(home, ".bazelrc");
    const lines = [`common --repository_cache=${repoCacheDir}`];
    if (diskCacheName) {
      lines.push(`common --disk_cache=${diskCacheDir}`);
    }
    fs.appendFileSync(bazelrc, lines.join("\n") + "\n");
    core.info(`Bazel cache paths written to ${bazelrc}`);

    core.info(`Cache setup complete (mode: ${mode}, disk-cache: ${diskCacheName || "(none)"}). Post hook registered for save.`);
  } catch (error) {
    core.setFailed(`Cache setup failed: ${error.message}`);
  }
}

run();
