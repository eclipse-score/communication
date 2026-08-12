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

const { execSync } = require("node:child_process");
const { appendFileSync } = require("node:fs");

function run(command) {
  execSync(command, { stdio: "inherit" });
}

function main() {
  const licenseDir = process.env["INPUT_LICENSE-DIR"] || "/opt/score_qnx/license";
  const qnxLicense = process.env["INPUT_QNX-LICENSE"] || "";
  const githubState = process.env.GITHUB_STATE;

  if (!qnxLicense) {
    throw new Error("Input 'qnx-license' is required.");
  }
  if (!githubState) {
    throw new Error("GITHUB_STATE is not available.");
  }

  appendFileSync(githubState, `LICENSE_DIR=${licenseDir}\n`, { encoding: "utf-8" });

  run("sudo apt-get update");
  run("sudo apt-get install -y qemu-system");
  run(
    "echo 'KERNEL==\"kvm\", GROUP=\"kvm\", MODE=\"0666\", OPTIONS+=\"static_node=kvm\"' | sudo tee /etc/udev/rules.d/99-kvm4all.rules",
  );
  run("sudo udevadm control --reload-rules");
  run("sudo udevadm trigger --name-match=kvm");

  const escapedLicenseDir = `'${licenseDir.replace(/'/g, `'\\''`)}'`;
  const escapedLicense = qnxLicense.replace(/'/g, `'\\''`);
  run(`sudo mkdir -p ${escapedLicenseDir}`);
  run(`echo '${escapedLicense}' | base64 --decode | sudo tee ${escapedLicenseDir}/licenses >/dev/null`);
}

main();
