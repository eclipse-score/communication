#!/bin/sh

export GCOV_PREFIX=/persistent/coverage
export GCOV_PREFIX_STRIP=3

mkdir /persistent/unit_tests

ROOT_DIR="/opt/tests/cc_test_qnx.runfiles/"

if [ -d "$ROOT_DIR" ]; then
    if [ -d "/opt/tests/cc_test_qnx.runfiles/safe_posix_platform" ]; then
        ROOT_DIR=/opt/tests/cc_test_qnx.runfiles/safe_posix_platform
    elif [ -d "/opt/tests/cc_test_qnx.runfiles/_main" ]; then
        ROOT_DIR=/opt/tests/cc_test_qnx.runfiles/_main
    elif [ -d "/opt/tests/cc_test_qnx.runfiles/ddad" ]; then
        ROOT_DIR=/opt/tests/cc_test_qnx.runfiles/ddad
    fi
    find ${ROOT_DIR} -maxdepth 1 -mindepth 1 -type d -exec cp -R '{}' /persistent/unit_tests/ \;
fi

export GTEST_FILTER="$(cat /opt/tests/cc_test_qnx_filters.txt)"

cd /persistent/unit_tests
ln -s -f /opt/tests/cc_test_qnx cc_test_qnx
/persistent/unit_tests/cc_test_qnx --gtest_output=xml:/persistent/test.xml

echo "$?" > /persistent/returncode.log

cp -fR /persistent/returncode.log /shared/returncode.log

if [ -e "/persistent/test.xml" ]; then
    cp -fR /persistent/test.xml /shared/test.xml
fi

# Wait for all test processes to finish
echo "Waiting for all test processes to finish..."
while pidin -F '%a %b %n' | grep cc_test_qnx > /dev/null 2>&1; do true; done
echo "Test processes finished"

if [ -d "/persistent/coverage" ]; then
    echo "Creating coverage archive..."
    time tar czf /shared/coverage.tar.gz -C /persistent/ coverage
    echo "Coverage archive created!"
fi

sync

cd /

umount /shared
umount /qnx6fs_persistent

shutdown
