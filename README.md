# communication
Repository for the communication module LoLa

## Building S-CORE communication with bazel

```shell
communication (main) $ bazel build ...
```

## IDE support

Enabling IDE support is the same on [all S-CORE devcontainers](https://github.com/eclipse-score/devcontainer/blob/main/README.md#usage).

After you have build the code, create databases via Visual Studio Code Tasks:

- C++: `Ctrl + p` -> `Tasks: Run Task` -> `Update compile_commands.json`
- Rust: `Ctrl + p` -> `Tasks: Run Task` -> `Generate rust-project.json`
