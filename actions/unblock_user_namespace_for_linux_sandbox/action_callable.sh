#!/bin/bash
INSTALL_BASE=$(bazel info install_base)
sudo bash -c "cat >/etc/apparmor.d/score-linux-sandbox" <<-EOF
abi <abi/4.0>,
include <tunables/global>

profile linux-sandbox ${INSTALL_BASE}/linux-sandbox flags=(unconfined) {
  userns,

  # Site-specific additions and overrides. See local/README for details.
  include if exists <local/score-linux-sandbox>
}
EOF
sudo apparmor_parser -r /etc/apparmor.d/score-linux-sandbox

${INSTALL_BASE}/linux-sandbox "/bin/true"
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
  echo "Warning: '${INSTALL_BASE}/linux-sandbox \"/bin/true\"' failed."
else
  echo "Success: '${INSTALL_BASE}/linux-sandbox \"/bin/true\"' succeeded."
fi
exit $EXIT_CODE
