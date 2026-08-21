# The supported LLVM releases. Each entry maps a requested version and host
# architecture to the upstream archive Bazel downloads and verifies.
_LLVM_DISTRIBUTIONS = {
    "19.1.0": {
        "aarch64": {
            # Official prebuilt LLVM package for 64-bit ARM Linux hosts.
            "url": "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/clang+llvm-19.1.0-aarch64-linux-gnu.tar.xz",
            # SHA-256 pinned by toolchains_llvm for this exact upstream archive.
            "sha256": "7bb54afd330fe1a1c2d4c593fa1e2dbe2abd9bf34fb3597994ff41e443cf144b",
        },
        "x86_64": {
            # Official prebuilt LLVM package for 64-bit x86 Linux hosts.
            "url": "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/LLVM-19.1.0-Linux-X64.tar.xz",
            # SHA-256 of that archive. `ctx.download` rejects altered or corrupt
            # downloads instead of unpacking them.
            "sha256": "cee77d641690466a193d9b88c89705de1c02bbad46bde6a3b126793c0a0f2923",
        },
    },
    "19.1.1": {
        "aarch64": {
            # Official prebuilt LLVM package for 64-bit ARM Linux hosts.
            "url": "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.1/clang+llvm-19.1.1-aarch64-linux-gnu.tar.xz",
            # SHA-256 pinned by toolchains_llvm for this exact upstream archive.
            "sha256": "414d2ebef10c5035e9df10a224e81b484dbe17d319373050d0c1b3b1467040d2",
        },
        "x86_64": {
            # Official prebuilt LLVM package for 64-bit x86 Linux hosts.
            "url": "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.1/LLVM-19.1.1-Linux-X64.tar.xz",
            # SHA-256 pinned by toolchains_llvm for this exact upstream archive.
            "sha256": "8204de000b6a6921f0572e038336601e3225898e9a253c8aaa43b0a5fae8a4ce",
        },
    },
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
    "amd64": "x86_64",
    "arm64": "aarch64",
    "x86_64": "x86_64",
}

def _log(message):
    print("[fast_llvm_repo] " + message)

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
    _log("downloading LLVM %s for %s" % (ctx.attr.llvm_version, host_arch))
    ctx.download(
        url = dist["url"],
        output = archive,
        sha256 = dist["sha256"],
    )
    _log("download completed")

    # Use the host's `tar` executable to unpack the xz archive. `ctx.which`
    # returns None instead of guessing when `tar` is unavailable.
    tar = ctx.which("tar")
    if not tar:
        fail("tar not found in PATH")

    # LLVM's archive has many independently decompressible XZ blocks. Let xz
    # decode them in parallel, then stream the resulting tar file directly to
    # tar; materializing the 8+ GiB uncompressed archive would be wasteful.
    # Keep the regular tar invocation for minimal Linux hosts without xz.
    xz = ctx.which("xz")
    if xz:
        _log("extracting LLVM with xz -T0 and tar")
        sh = ctx.which("sh")
        if not sh:
            fail("sh not found in PATH")
        result = ctx.execute(
            [
                sh,
                "-c",
                "set -e; \"$1\" -T0 -dc \"$2\" | \"$3\" -xf - --strip-components=1 -C \"$4\"",
                "fast_llvm_repo",
                xz,
                archive,
                tar,
                ctx.path("."),
            ],
            # Allow the relatively large archive up to 30 minutes to unpack.
            timeout = 1800,
            # Do not suppress command output from Bazel's repository-rule log.
            quiet = False,
        )
    else:
        _log("xz not found; extracting LLVM with tar fallback")
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
            timeout = 1800,
            quiet = False,
        )

    # Stop repository creation if extraction reported an error.
    if result.return_code:
        fail(result.stderr)
    _log("extraction completed")

    # The archive is no longer needed after extraction; leave only the LLVM
    # distribution files in the external repository.
    ctx.delete(archive)

    # Generate the BUILD file expected by toolchains_llvm 1.7. LLVM 16 and
    # later store compiler resources in a directory named by the major version.
    _log("generating BUILD.bazel")
    ctx.template(
        "BUILD.bazel",
        Label("@toolchains_llvm//toolchain:BUILD.llvm_repo.tpl"),
        substitutions = {"{LLVM_VERSION}": ctx.attr.llvm_version.split(".")[0]},
    )
    _log("llvm setup completed")

# Public repository rule used from MODULE.bazel. Its only user-facing input
# is an LLVM version.
fast_llvm_repo = repository_rule(
    implementation = _fast_llvm_repo_impl,
    attrs = {
        # Required version key used to select an entry in `_LLVM_DISTRIBUTIONS`.
        "llvm_version": attr.string(mandatory = True),
    },
)
