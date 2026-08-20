# The supported LLVM releases. Each entry maps a requested version and host
# architecture to the upstream archive Bazel downloads and verifies.
_LLVM_DISTRIBUTIONS = {
    "22.1.7": {
        "aarch64": {
            # Official prebuilt LLVM package for 64-bit ARM Linux hosts.
            "url": "https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.7/LLVM-22.1.7-Linux-ARM64.tar.xz",
            # SHA-256 from toolchains_llvm's distribution metadata for this
            # exact upstream archive. `ctx.download` rejects altered or corrupt
            # downloads.
            "sha256": "118ca2d3ad9da34367e05735317854e7977db45dc4c02a32af58da64c23b8789",
        },
        "x86_64": {
            # Official prebuilt LLVM package for 64-bit x86 Linux hosts.
            "url": "https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.7/LLVM-22.1.7-Linux-X64.tar.xz",
            "sha256": "edb0522b41e261819c06ea437d249f9b8acfa413d3805bc9920eec6fb76ff830",
        },
    },
}

_ARCH_ALIASES = {
    "aarch64": "aarch64",
    "arm64": "aarch64",
    "amd64": "x86_64",
    "x86_64": "x86_64",
}

# Implementation of `fast_llvm_repo`. Repository rules run while Bazel is
# preparing external dependencies, before analysing or building project code.
def _fast_llvm_repo_impl(ctx):
    # Select the archive native to the host which will execute this toolchain.
    host_arch = _ARCH_ALIASES.get(ctx.os.arch)
    if not host_arch:
        fail("Unsupported host architecture: %s" % ctx.os.arch)

    # Look up the requested release and its host-specific archive.
    distributions = _LLVM_DISTRIBUTIONS.get(ctx.attr.llvm_version)
    dist = distributions.get(host_arch) if distributions else None

    # Fail with a useful message if MODULE.bazel asks for a version that has no
    # URL and checksum entry yet.
    if not dist:
        fail("Unsupported LLVM version/host architecture: %s/%s" % (ctx.attr.llvm_version, host_arch))

    # Choose a temporary file inside this external repository for the archive.
    archive = ctx.path("llvm.tar.xz")

    # Download the archive and verify its SHA-256 before using it.
    ctx.download(
        url = dist["url"],
        output = archive,
        sha256 = dist["sha256"],
    )

    # Use the host's `tar` executable to unpack the xz archive. `ctx.which`
    # returns None instead of guessing when `tar` is unavailable.
    tar = ctx.which("tar")
    if not tar:
        fail("tar not found in PATH")

    # Unpack the distribution into the root of this external repository.
    # `--strip-components=1` removes LLVM's top-level directory so paths such
    # as `bin/clang` are directly under `@llvm_toolchain_llvm`.
    result = ctx.execute(
        [
            tar,
            "-xf",
            archive,
            "--strip-components=1",
            # Extract into the external repository represented by `ctx`.
            "-C",
            ctx.path("."),
        ],
        # Allow the relatively large archive up to 30 minutes to unpack.
        timeout = 1800,
        # Do not suppress `tar` output from Bazel's repository-rule log.
        quiet = False,
    )

    # Stop repository creation if `tar` reported an extraction error.
    if result.return_code:
        fail(result.stderr)

    # The archive is no longer needed after extraction; leave only the LLVM
    # distribution files in the external repository.
    ctx.delete(archive)

    # Generate the BUILD file expected by toolchains_llvm 1.8.  LLVM 16 and
    # later store compiler resources in a directory named by the major version.
    ctx.template(
        "BUILD.bazel",
        Label("@toolchains_llvm//toolchain:BUILD.llvm_repo.tpl"),
        substitutions = {"{LLVM_VERSION}": ctx.attr.llvm_version.split(".")[0]},
    )

# Public repository rule used from MODULE.bazel. Its only user-facing input
# is an LLVM version.
fast_llvm_repo = repository_rule(
    implementation = _fast_llvm_repo_impl,
    attrs = {
        # Required version key used to select an entry in `_LLVM_DISTRIBUTIONS`.
        "llvm_version": attr.string(mandatory = True),
    },
)
