import os
import sys
import tarfile
from fs import open_fs

def check_tests_result() -> int:
    shared_img_path = sys.argv[1]
    shared_img = open_fs("fat://" + shared_img_path + "?preserve_case=true")

    output_xml = os.environ.get("XML_OUTPUT_FILE")
    output_dir = os.environ.get("TEST_UNDECLARED_OUTPUTS_DIR")
    coverage = os.environ.get("COVERAGE", "0") == "1"

    if shared_img.exists("test.xml"):
        with open(output_xml, 'wb') as file:
            shared_img.download('test.xml', file)

    if coverage:
        coverage_archive = output_dir + "/coverage.tar.gz"
        if shared_img.exists("coverage.tar.gz"):
            with open(coverage_archive, 'wb') as file:
                shared_img.download('coverage.tar.gz', file)

            with tarfile.open(coverage_archive) as tf:
                tf.extractall(output_dir)

    return int(shared_img.readtext('returncode.log'))

if __name__ == "__main__":
    sys.exit(check_tests_result())
