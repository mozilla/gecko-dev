# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import pickle
import sys
from pathlib import Path

import cpp
import jinja2
import jog
import rust
from buildconfig import topsrcdir
from glean_parser import lint, metrics, parser, translate, util
from metrics_header_names import convert_yaml_path_to_header_name
from mozbuild.util import FileAvoidWrite, memoize
from util import generate_metric_ids

import js


@memoize
def get_deps():
    # Any imported python module is added as a dep automatically,
    # so we only need the index and the templates.
    return {
        *[str(p) for p in (Path(os.path.dirname(__file__)) / "templates").iterdir()],
    }


class ParserError(Exception):
    """Thrown from parse if something goes wrong"""

    pass


GIFFT_TYPES = {
    "Event": ["event"],
    "Histogram": [
        "custom_distribution",
        "labeled_custom_distribution",
        "memory_distribution",
        "labeled_memory_distribution",
        "timing_distribution",
        "labeled_timing_distribution",
        "counter",
        "labeled_counter",
    ],
    "Scalar": [
        "boolean",
        "labeled_boolean",
        "counter",
        "labeled_counter",
        "string",
        "string_list",
        "timespan",
        "uuid",
        "datetime",
        "quantity",
        "labeled_quantity",
        "rate",
        "url",
    ],
}


def get_parser_options(moz_app_version, is_local_build):
    app_version_major = moz_app_version.split(".", 1)[0]
    return {
        "allow_reserved": False,
        "expire_by_version": int(app_version_major),
        "is_local_build": is_local_build,
    }


def parse(args, interesting_yamls=None):
    """
    Parse and lint the input files,
    then return the parsed objects for further processing.

    :param interesting_yamls: If set, the "opt-in" list of metrics to actually
      collect. Other metrics not listed in files in this list will be marked
      disabled and thus not collected (only built).
    """

    fast_rebuild = args[-1] == "--fast-rebuild"
    if fast_rebuild:
        args = args[:-1]

    yaml_array = args[:-1]
    if all(arg.endswith(".cached") for arg in yaml_array):
        objects = dict()
        options = None
        for cache_file in yaml_array:
            with open(cache_file, "rb") as cache:
                cached_objects, cached_options = pickle.load(cache)
                objects.update(cached_objects)
                assert (
                    options is None or cached_options == options
                ), "consistent options"
                options = options or cached_options
        return objects, options

    # Unfortunately, GeneratedFile appends `flags` directly after `inputs`
    # instead of listifying either, so we need to pull stuff from a *args.
    moz_app_version = args[-1]
    input_files = [Path(x) for x in yaml_array]

    options = get_parser_options(moz_app_version, fast_rebuild)
    if interesting_yamls:
        # We need to make these paths absolute here. They are used from at least
        # two different contexts.
        interesting = [Path(os.path.join(topsrcdir, x)) for x in interesting_yamls]
        options.update({"interesting": interesting})

    return parse_with_options(input_files, options)


def parse_with_options(input_files, options):
    # Derived heavily from glean_parser.translate.translate.
    # Adapted to how mozbuild sends us a fd, and to expire on versions not dates.

    all_objs = parser.parse_objects(input_files, options)
    if util.report_validation_errors(all_objs):
        raise ParserError("found validation errors during parse")

    nits = lint.lint_metrics(all_objs.value, options)
    if nits is not None and any(nit.check_name != "EXPIRED" for nit in nits):
        # Treat Warnings as Errors in FOG.
        # But don't fail the whole build on expired metrics (it blocks testing).
        raise ParserError("glinter nits found during parse")

    objects = all_objs.value

    translate.transform_metrics(objects)

    return objects, options


def main(cpp_fd, *args):
    def open_output(filename):
        return FileAvoidWrite(os.path.join(os.path.dirname(cpp_fd.name), filename))

    all_objs, options = parse(args)
    all_metric_header_files = {}

    for category_name in all_objs.keys():
        if category_name in ["pings", "tags"]:
            continue
        for name, metric in all_objs[category_name].items():
            filepath = metric.defined_in["filepath"].replace("\\", "/")
            if not (filepath.startswith(topsrcdir) and filepath.endswith(".yaml")):
                raise ParserError("Unexpected path" + filepath)

            filename = convert_yaml_path_to_header_name(filepath[len(topsrcdir) + 1 :])
            if not filename in all_metric_header_files:
                all_metric_header_files[filename] = {}
            if not category_name in all_metric_header_files[filename]:
                all_metric_header_files[filename][category_name] = {}
            all_metric_header_files[filename][category_name][name] = metric

    get_metric_id = generate_metric_ids(all_objs, options)
    for header_name, objs in all_metric_header_files.items():
        cpp.output_cpp(
            objs,
            (
                cpp_fd
                if header_name == "GleanMetrics"
                else open_output(header_name + ".h")
            ),
            {"header_name": header_name, "get_metric_id": get_metric_id},
        )

    return get_deps()


def js_metrics(js_cpp_fd, *args):
    def open_output(filename):
        return FileAvoidWrite(os.path.join(os.path.dirname(js_cpp_fd.name), filename))

    js_h_path = args[-1]
    args = args[:-1]
    all_objs, options = parse(args)

    with open_output(js_h_path) as js_fd:
        js.output_js(all_objs, js_fd, js_cpp_fd, options)

    return get_deps()


def rust_metrics(rust_fd, *args):
    all_objs, options = parse(args)

    # We only need this info if we're dealing with pings.
    ping_names_by_app_id = {}
    rust.output_rust(all_objs, rust_fd, ping_names_by_app_id, options)

    return get_deps()


def pings(cpp_fd, *args):
    def open_output(filename):
        return FileAvoidWrite(os.path.join(os.path.dirname(cpp_fd.name), filename))

    [js_h_path, js_cpp_path, rust_path] = args[-3:]
    args = args[:-3]
    all_objs, options = parse(args)

    cpp.output_cpp(all_objs, cpp_fd, options)

    with open_output(js_h_path) as js_fd:
        with open_output(js_cpp_path) as js_cpp_fd:
            js.output_js(all_objs, js_fd, js_cpp_fd, options)

    ping_names_by_app_id = {}
    from os import path

    sys.path.append(path.join(path.dirname(__file__), path.pardir, path.pardir))
    from metrics_index import pings_by_app_id

    for app_id, ping_yamls in pings_by_app_id.items():
        input_files = [Path(path.join(topsrcdir, x)) for x in ping_yamls]
        ping_objs, _ = parse_with_options(input_files, options)
        ping_names_by_app_id[app_id] = sorted(ping_objs["pings"].keys())

    with open_output(rust_path) as rust_fd:
        rust.output_rust(all_objs, rust_fd, ping_names_by_app_id, options)

    return get_deps()


def gifft_map(output_fd, *args):
    probe_type = args[-1]
    args = args[:-1]
    all_objs, options = parse(args)

    # Events also need to output maps from event extra enum to strings.
    # Sadly we need to generate code for all possible events, not just mirrored.
    # Otherwise we won't compile.
    if probe_type == "Event":
        output_path = Path(os.path.dirname(output_fd.name))
        with FileAvoidWrite(output_path / "EventExtraGIFFTMaps.cpp") as cpp_fd:
            output_gifft_map(output_fd, probe_type, all_objs, cpp_fd, options)
    else:
        output_gifft_map(output_fd, probe_type, all_objs, None, options)

    return get_deps()


def output_gifft_map(output_fd, probe_type, all_objs, cpp_fd, options):
    get_metric_id = generate_metric_ids(all_objs, options)
    ids_to_probes = {}
    for category_name, objs in all_objs.items():
        for metric in objs.values():
            if (
                hasattr(metric, "telemetry_mirror")
                and metric.telemetry_mirror is not None
            ):
                if metric.type in ["counter", "labeled_counter"]:
                    # These types map to Scalars... unless prefixed with `h#`,
                    # then they map to Histograms.
                    if (
                        probe_type == "Histogram"
                        and not metric.telemetry_mirror.startswith("h#")
                        or probe_type != "Histogram"
                        and metric.telemetry_mirror.startswith("h#")
                    ):
                        continue
                info = (
                    metric.telemetry_mirror.split("#")[-1],
                    f"{category_name}.{metric.name}",
                )
                if metric.type in GIFFT_TYPES[probe_type]:
                    if any(
                        metric.telemetry_mirror == value[0]
                        for value in ids_to_probes.values()
                    ):
                        print(
                            f"Telemetry mirror {metric.telemetry_mirror} already registered",
                            file=sys.stderr,
                        )
                        sys.exit(1)
                    ids_to_probes[get_metric_id(metric)] = info
                # If we don't support a mirror for this metric type: build error.
                elif not any(
                    [
                        metric.type in types_for_probe
                        for types_for_probe in GIFFT_TYPES.values()
                    ]
                ):
                    print(
                        f"Glean metric {category_name}.{metric.name} is of type {metric.type}"
                        " which can't be mirrored (we don't know how).",
                        file=sys.stderr,
                    )
                    sys.exit(1)
                # We only support mirrors for lifetime: ping
                # If you understand and are okay with how Legacy Telemetry has no
                # mechanism to which to mirror non-ping lifetimes,
                # you may use `no_lint: [GIFFT_NON_PING_LIFETIME]`
                elif (
                    metric.lifetime != metrics.Lifetime.ping
                    and "GIFFT_NON_PING_LIFETIME" not in metric.no_lint
                ):
                    print(
                        f"Glean lifetime semantics are not mirrored. {category_name}.{metric.name}'s lifetime of {metric.lifetime} is not supported.",
                        file=sys.stderr,
                    )
                    sys.exit(1)

    env = jinja2.Environment(
        loader=jinja2.PackageLoader("run_glean_parser", "templates"),
        trim_blocks=True,
        lstrip_blocks=True,
    )
    env.filters["snake_case"] = lambda value: value.replace(".", "_").replace("-", "_")
    env.filters["Camelize"] = util.Camelize
    template = env.get_template("gifft.jinja2")
    output_fd.write(
        template.render(
            ids_to_probes=ids_to_probes,
            probe_type=probe_type,
            id_bits=js.ID_BITS,
            id_signal_bits=js.ID_SIGNAL_BITS,
            runtime_metric_bit=jog.RUNTIME_METRIC_BIT,
        )
    )
    output_fd.write("\n")


def jog_factory(output_fd, *args):
    all_objs, options = parse(args)
    jog.output_factory(all_objs, output_fd, options)
    return get_deps()


def jog_file(output_fd, *args):
    all_objs, options = parse(args)
    jog.output_file(all_objs, output_fd, options)
    return get_deps()


def ohttp_pings(output_fd, *args):
    all_objs, options = parse(args)
    ohttp_pings = []
    for ping in all_objs["pings"].values():
        if ping.metadata.get("use_ohttp", False):
            if ping.include_info_sections:
                raise ParserError(
                    "Cannot send pings with OHTTP that contain {client|ping}_info sections. Specify `metadata: include_info_sections: false`"
                )
            ohttp_pings.append(ping.name)

    env = jinja2.Environment(
        loader=jinja2.PackageLoader("run_glean_parser", "templates"),
        trim_blocks=True,
        lstrip_blocks=True,
    )
    env.filters["quote_and_join"] = lambda l: "\n| ".join(f'"{x}"' for x in l)
    template = env.get_template("ohttp.jinja2")
    output_fd.write(
        template.render(
            ohttp_pings=ohttp_pings,
        )
    )
    output_fd.write("\n")
    return get_deps()


if __name__ == "__main__":
    main(sys.stdout, *sys.argv[1:])
