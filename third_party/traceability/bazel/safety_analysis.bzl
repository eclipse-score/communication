load("@lobster//:lobster.bzl", "lobster_test", "lobster_trlc")
load("//bazel/rules:expand_template.bzl", "expand_template")
load("//third_party/traceability/tools/source_code_linker:collect_source_files.bzl", "parse_source_files_for_needs_links")

def safety_analysis(
        name,
        failuremodes = None,
        fta = None,
        controlmeasures = None):
    FMEA_FAILUREMODES = "{}_failuremodes_lobster".format(name)
    FTA_TOPLEVELEVENTS = "{}_fta_tle".format(name)
    FTA_BASICEVENTS = "{}_fta_be".format(name)
    FMEA_CONTROLMEASURES = "{}_controlmeasures_lobster".format(name)

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

    parse_source_files_for_needs_links(
        name = FTA_TOPLEVELEVENTS,
        srcs_and_deps = fta,
        trace_nodes = ["$TopEvent"],
    )

    parse_source_files_for_needs_links(
        name = FTA_BASICEVENTS,
        srcs_and_deps = fta,
        trace = "reqs",
        trace_nodes = ["$BasicEvent"],
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
        ],
    )
