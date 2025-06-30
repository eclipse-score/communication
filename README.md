# communication
Repository for the communication module LoLa

## Building S-CORE communication with bazel

```shell
communication (main) $ bazel build ...
```

## IDE support (compile_commands.json)

[bazel-compile-commands](https://github.com/kiron1/bazel-compile-commands) creates a `compile_commands.json` file which can then be used by IDEs / clangd for code completion and navigation.
The container image has this tool preinstalled.

To create a `compile_commands.json` file, run the following commands:

```shell
communication (main) $ bazel build ...
communication (main) $ bazel-compile-commands ...
```

### Visual Studio Code

After both commands were run successfully, press in Visual Studio Code `ctrl + shift p` and select `clangd: Restart language server`.
