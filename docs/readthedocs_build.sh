#!/usr/bin/env bash
# Taken over from https://github.com/bazel-contrib/rules_python/blob/2e8aa91/docs/readthedocs_build.sh

set -eou pipefail

declare -a extra_env
while IFS='=' read -r -d '' name value; do
  if [[ "$name" == READTHEDOCS* ]]; then
    extra_env+=("--@rules_python//sphinxdocs:extra_env=$name=$value")
  fi
done < <(env -0)

# In order to get the build number, we extract it from the host name
extra_env+=("--@rules_python//sphinxdocs:extra_env=HOSTNAME=$HOSTNAME")

set -x
bazel run \
  "--@rules_python//sphinxdocs:extra_defines=version=$READTHEDOCS_VERSION" \
  "${extra_env[@]}" \
  //docs/sphinx:readthedocs_install