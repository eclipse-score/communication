"""Sphinx-build wrapper that translates Bazel arguments to sphinx format.
This is necessary because the sphinx_build_binary provided by rules_python
does not support all the arguments we need."""
import sys

if __name__ == "__main__":
    args = []
    builder = "html"
    i = 1
    source_dir = None
    output_dir = None

    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "--builder":
            i += 1
            builder = sys.argv[i]
        elif arg == "--show-traceback":
            args.append("-T")
        elif arg == "--quiet":
            args.append("-q")
        elif arg == "--write-all":
            args.append("-a")
        elif arg == "--fresh-env":
            args.append("-E")
        elif arg == "--jobs":
            i += 1
            args.extend(["-j", sys.argv[i]])
        elif not arg.startswith("-"):
            if source_dir is None:
                source_dir = arg
            elif output_dir is None:
                output_dir = arg
        i += 1

    final_args = ["-b", builder] + args + [source_dir, output_dir]

    from sphinx.cmd.build import main
    raise SystemExit(main(final_args))
