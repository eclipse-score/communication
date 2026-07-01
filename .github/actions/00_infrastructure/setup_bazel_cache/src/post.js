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

const core = require("@actions/core");
const cache = require("@actions/cache");
const exec = require("@actions/exec");
const github = require("@actions/github");
const path = require("path");
const os = require("os");
const fs = require("fs");
const crypto = require("crypto");

async function computeRepoCacheHash(repoCacheDir) {
  const caDir = path.join(repoCacheDir, "content_addressable", "sha256");
  let entries = [];
  if (fs.existsSync(caDir)) {
    entries = fs.readdirSync(caDir).sort();
  }
  if (entries.length === 0) {
    throw new Error(`Repository cache content_addressable/sha256/ directory is empty or missing at ${caDir}. Cannot compute cache hash.`);
  }
  const hash = crypto.createHash("sha256");
  for (const entry of entries) {
    hash.update(entry);
    hash.update("\n");
  }
  return hash.digest("hex");
}

async function deleteOldCaches(prefix, currentKey) {
  const token = core.getState("github-token") || process.env.GITHUB_TOKEN || "";
  if (!token) {
    core.warning("No GITHUB_TOKEN available, skipping old cache cleanup.");
    return;
  }

  const octokit = github.getOctokit(token);
  const [owner, repo] = (process.env.GITHUB_REPOSITORY || "").split("/");
  if (!owner || !repo) return;

  try {
    const { data } = await octokit.rest.actions.getActionsCacheList({
      owner,
      repo,
      key: prefix,
    });

    for (const entry of data.actions_caches || []) {
      if (entry.key !== currentKey) {
        core.info(`Deleting old cache: ${entry.key} (id=${entry.id})`);
        await octokit.rest.actions.deleteActionsCacheById({
          owner,
          repo,
          cache_id: entry.id,
        }).catch(() => {});
      }
    }
  } catch (error) {
    core.warning(`Cache cleanup failed: ${error.message}`);
  }
}

async function run() {
  try {
    const mode = core.getState("cache-mode");
    const diskCacheName = core.getState("disk-cache-name");

    if (!mode || mode === "disabled" || mode === "read-only") {
      core.info(`Cache mode '${mode}' — skipping save.`);
      return;
    }

    const home = os.homedir();
    const repoCacheDir = path.join(home, ".cache", "bazel", "repository_cache");
    const diskCacheDir = path.join(home, ".cache", "bazel", "disk_cache");
    const runId = process.env.GITHUB_RUN_ID || "0";

    // Save disk cache (update-disk, recreate, recreate-update)
    if (diskCacheName) {
      const diskKey = `disk-cache-${diskCacheName}-${runId}`;
      core.info(`Saving disk cache with key: ${diskKey}`);
      try {
        await cache.saveCache([diskCacheDir], diskKey);
        core.info("Disk cache saved.");
        await deleteOldCaches(`disk-cache-${diskCacheName}-`, diskKey);
      } catch (error) {
        if (error.name === "ReserveCacheError" || error.message?.includes("Unable to reserve cache")) {
          core.info("Disk cache already exists, skipping save.");
        } else {
          core.warning(`Failed to save disk cache: ${error.message}`);
        }
      }
    }

    // Save repository cache (recreate, recreate-update only)
    if (mode === "recreate" || mode === "recreate-update") {
      const hash = await computeRepoCacheHash(repoCacheDir);
      const repoKey = `repo-cache-${hash}`;
      core.info(`Saving repository cache with key: ${repoKey}`);
      try {
        await cache.saveCache([repoCacheDir], repoKey);
        core.info("Repository cache saved.");
        await deleteOldCaches("repo-cache-", repoKey);
      } catch (error) {
        if (error.name === "ReserveCacheError" || error.message?.includes("Unable to reserve cache")) {
          core.info("Repository cache already exists (same content hash), skipping save.");
        } else {
          core.warning(`Failed to save repository cache: ${error.message}`);
        }
      }
    }

    core.info("Cache save complete.");
  } catch (error) {
    core.warning(`Post step failed: ${error.message}`);
  }
}

run();
