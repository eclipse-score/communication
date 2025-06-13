ARG BASE_IMAGE=debian:12

FROM ${BASE_IMAGE}
USER root

# Optimization of installs
ENV DEBIAN_FRONTEND=noninteractive

# create a user developer
RUN apt-get update && apt-get install -y sudo && \
    adduser developer --quiet && \
    adduser developer sudo && \
    echo "%sudo  ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

# Definition of arguments for GCC and LLVM Version
ARG GCC_VERSION=11
ARG LLVM_VERSION=16

# Installation of c++ and utility libraries
# For C++ -> g++ compiler (in specified version), clang-tidy & clang-format (in specified LLVM version), googletest & gcovr for c++ testing
# For Detailed design -> graphviz default-jre (we can preview puml files)
# Utilities -> git, git-lfs and wget
RUN apt-get update \
    && apt-get -y install wget git git-lfs graphviz default-jre libtinfo5 \
    && apt-get -y install g++-${GCC_VERSION} gdb googletest gcovr cmake \
    && apt-get -y install clang-${LLVM_VERSION} clang-tidy-${LLVM_VERSION} clang-format-${LLVM_VERSION} clangd-${LLVM_VERSION} \
    && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*

# specifying default versions via update alternatives functionality
RUN update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-${LLVM_VERSION} 100 && update-alternatives --set clang-tidy /usr/bin/clang-tidy-${LLVM_VERSION}
RUN update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-${LLVM_VERSION} 100 && update-alternatives --set clang-format /usr/bin/clang-format-${LLVM_VERSION}
RUN update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-${LLVM_VERSION} 100 && update-alternatives --set clangd /usr/bin/clangd-${LLVM_VERSION}
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 100 && update-alternatives --set g++ /usr/bin/g++-${GCC_VERSION}
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 100 && update-alternatives --set gcc /usr/bin/gcc-${GCC_VERSION}

# installation of bazelisk, a bazel versioning tool
RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.26.0/bazelisk-linux-amd64 && chmod +x bazelisk-linux-amd64 && mv bazelisk-linux-amd64 /usr/local/bin/bazel
RUN wget https://github.com/bazelbuild/buildtools/releases/download/v8.2.1/buildifier-linux-amd64 && chmod +x buildifier-linux-amd64 && mv buildifier-linux-amd64 /usr/local/bin/buildifier

# Code completion for C++ code of bazel projects
RUN wget https://github.com/kiron1/bazel-compile-commands/releases/download/v0.17.2/bazel-compile-commands_0.17.2-jammy_amd64.deb
RUN dpkg -i bazel-compile-commands_0.17.2-jammy_amd64.deb && rm bazel-compile-commands_0.17.2-jammy_amd64.deb

# Ignore GCC only options
USER developer
RUN mkdir -p ~/.config/clangd && \
    echo "CompileFlags:" >> ~/.config/clangd/config.yaml && \
    echo "  Remove:" >> ~/.config/clangd/config.yaml && \
    echo "    - -fno-canonical-system-headers" >> ~/.config/clangd/config.yaml
