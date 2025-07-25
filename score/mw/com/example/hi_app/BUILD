
genrule(
    name = "ipc_bridge_cpp_sil",
    srcs = ["//score/mw/com/example/ipc_bridge:ipc_bridge_cpp"],
    outs = ["ipc_bridge_cpp_sil"],
    cmd = "cp $(SRCS) $@ && \
     chmod ugo+w $@ && \
     $(execpath @gcc_toolchain_gcc//:elf-enabler) $@ && \
     chmod ugo-w $@",
    tools = ["@gcc_toolchain_gcc//:elf-enabler"],
)
