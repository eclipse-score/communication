filegroup(
    name = "config_schemas_files",
    srcs = [
        "//score/mw/com/gateway/gateway_application/configuration:mw_com_gateway_config_schema",
        "//score/mw/com/gateway/transport_layer/sample/configuration:mw_com_gateway_sample_transport_config_schema",
        "//score/mw/com/impl/configuration:mw_com_config_schema",
        "//score/mw/com/impl/tracing/configuration:comtrace_filter_config_schema",
    ],
    visibility = ["//visibility:private"],
)

alias(
    name = "config_schemas",
    actual = ":config_schemas_files",
    visibility = ["//visibility:public"],
)