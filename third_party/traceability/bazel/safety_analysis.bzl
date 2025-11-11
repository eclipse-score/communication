load("@lobster//:lobster.bzl", "lobster_test", "lobster_trlc")
load("//bazel/rules:expand_template.bzl", "expand_template")
load("//third_party/traceability/tools/source_code_linker:collect_source_files.bzl", "parse_source_files_for_links")

def safety_analysis(
        name,
        failuremodes = None,
        fta = None,
        controlmeasures = None,
        public_api = None):
    FMEA_FAILUREMODES = "{}_failuremodes_lobster".format(name)
    FTA_TOPLEVELEVENTS = "{}_fta_tle".format(name)
    FTA_BASICEVENTS = "{}_fta_be".format(name)
    FMEA_CONTROLMEASURES = "{}_controlmeasures_lobster".format(name)
    PUBLIC_API = "{}_public_api".format(name)

    lobster_trlc(
        name = FMEA_FAILUREMODES,
        config = "@//third_party/traceability/config/lobster:safety_analysis_conf",
        requirements = failuremodes,
    )

    lobster_trlc(
        name = FMEA_CONTROLMEASURES,
        config = "@//third_party/traceability/config/lobster:safety_analysis_conf",
        requirements = controlmeasures,
    )

    parse_source_files_for_links(
        name = FTA_TOPLEVELEVENTS,
        srcs_and_deps = fta,
        trace = "plantuml_alias_cpp",
        trace_tags = ["$TopEvent"],
    )

    parse_source_files_for_links(
        name = FTA_BASICEVENTS,
        srcs_and_deps = fta,
        trace = "plantuml_alias_req",
        trace_tags = ["$BasicEvent"],
    )

    parse_source_files_for_links(
        name = PUBLIC_API,
        srcs_and_deps = public_api,
        trace = "plantuml",
        trace_tags = ["interface"],
    )

    LOBSTER_CONFIG = "{}_lobster_config".format(name)
    expand_template(
        name = LOBSTER_CONFIG,
        template = "@//third_party/traceability/config/lobster:traceability_safety_analysis_conf",
        out = "{}_traceability_config".format(name),
        substitutions = {
            "fmea_control_measures": FMEA_CONTROLMEASURES,
            "fmea_failure_modes": FMEA_FAILUREMODES,
            "fta_basic": FTA_BASICEVENTS,
            "fta_toplevel": FTA_TOPLEVELEVENTS,
            "public_api": PUBLIC_API,
        },
    )

    lobster_test(
        name = name,
        config = LOBSTER_CONFIG,
        inputs = [
            FMEA_FAILUREMODES,
            FTA_TOPLEVELEVENTS,
            FTA_BASICEVENTS,
            FMEA_CONTROLMEASURES,
            PUBLIC_API,
        ],
    )
