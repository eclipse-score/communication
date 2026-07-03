#!/bin/bash
# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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

# Compares current API surface against a lock file.
# Exit code 0 = match, exit code 1 = mismatch.
#
# Usage: diff_api.sh <lock_file> <current_file> [--check-docs]

set -euo pipefail

LOCK_FILE="$1"
CURRENT_FILE="$2"
CHECK_DOCS="${3:-}"

# Check for undocumented public symbols
if [[ "$CHECK_DOCS" == "--check-docs" ]]; then
    UNDOCUMENTED=$(python3 -c "
import json, sys
with open('$CURRENT_FILE') as f:
    data = json.load(f)
undoc = data.get('undocumented_symbols', [])
if undoc:
    print('ERROR: The following public symbols are missing \\\\api documentation:')
    print()
    for s in undoc:
        print(f'  {s[\"qualified_name\"]} ({s[\"kind\"]})')
        print(f'    at {s[\"file\"]}:{s[\"line\"]}')
    print()
    print(f'Total: {len(undoc)} undocumented public symbol(s)')
    print()
    print('Fix: Add \\\\api and \\\\brief doxygen tags to these declarations.')
    sys.exit(1)
" 2>&1) || {
        echo "$UNDOCUMENTED"
        echo ""
        exit 1
    }
fi

if [[ ! -f "$LOCK_FILE" ]]; then
    echo "ERROR: Lock file not found: $LOCK_FILE"
    echo ""
    echo "This likely means the API surface lock file has not been generated yet."
    echo "Run the update target to generate it:"
    echo ""
    echo "  bazel run //score/mw/com:api_surface_update"
    echo "  (or the corresponding _update target for your library)"
    echo ""
    exit 1
fi

# Compare only the documented symbols (the API contract)
# Extract just the symbols array for comparison
LOCK_SYMBOLS=$(python3 -c "
import json
with open('$LOCK_FILE') as f:
    data = json.load(f)
# Normalize: only compare name, qualified_name, kind, signature
symbols = [{'name': s['name'], 'qualified_name': s['qualified_name'], 'kind': s['kind'], 'signature': s['signature']} for s in data.get('symbols', [])]
print(json.dumps(symbols, indent=2, sort_keys=True))
")

CURRENT_SYMBOLS=$(python3 -c "
import json
with open('$CURRENT_FILE') as f:
    data = json.load(f)
# Normalize: only compare name, qualified_name, kind, signature
symbols = [{'name': s['name'], 'qualified_name': s['qualified_name'], 'kind': s['kind'], 'signature': s['signature']} for s in data.get('symbols', [])]
print(json.dumps(symbols, indent=2, sort_keys=True))
")

if [[ "$LOCK_SYMBOLS" == "$CURRENT_SYMBOLS" ]]; then
    echo "API surface check PASSED: No changes detected."
    exit 0
fi

echo "ERROR: API surface has changed!"
echo ""
echo "Differences between lock file and current API:"
echo ""
diff <(echo "$LOCK_SYMBOLS") <(echo "$CURRENT_SYMBOLS") || true
echo ""
echo "If this change is intentional, update the lock file:"
echo ""
echo "  bazel run //score/mw/com:api_surface_update"
echo ""
echo "  (or the corresponding _update target for your library)"
echo ""
exit 1
