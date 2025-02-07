# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate C++ code for metrics.
"""

import jinja2
from glean_parser import metrics, util
from mozbuild.util import memoize
from util import generate_metric_ids, generate_ping_ids, get_metrics


def type_name(obj):
    """
    Returns the C++ type to use for a given metric object.
    """

    if getattr(obj, "labeled", False):
        class_name = util.Camelize(obj.type[8:])  # strips "labeled_" off the front.
        label_enum = "DynamicLabel"
        if obj.labels and len(obj.labels):
            label_enum = f"{util.Camelize(obj.name)}Label"
        return f"Labeled<impl::{class_name}Metric, {label_enum}>"
    generate_enums = getattr(obj, "_generate_enums", [])  # Extra Keys? Reasons?
    if len(generate_enums):
        for name, _ in generate_enums:
            if not len(getattr(obj, name)) and isinstance(obj, metrics.Event):
                return util.Camelize(obj.type) + "Metric<NoExtraKeys>"
            else:
                # we always use the `extra` suffix,
                # because we only expose the new event API
                suffix = "Extra"
                return "{}Metric<{}>".format(
                    util.Camelize(obj.type), util.Camelize(obj.name) + suffix
                )
    return util.Camelize(obj.type) + "Metric"


def extra_type_name(typ: str) -> str:
    """
    Returns the corresponding Rust type for event's extra key types.
    """

    if typ == "boolean":
        return "bool"
    elif typ == "string":
        return "nsCString"
    elif typ == "quantity":
        return "uint32_t"
    else:
        return "UNSUPPORTED"


@memoize
def get_metrics_template(get_metric_id):
    return util.get_jinja2_template(
        "cpp.jinja2",
        filters=(
            ("snake_case", lambda value: value.replace(".", "_").replace("-", "_")),
            ("type_name", type_name),
            ("extra_type_name", extra_type_name),
            ("metric_id", get_metric_id),
        ),
    )


def output_cpp(objs, output_fd, options={}):
    """
    Given a tree of objects, output C++ code to the file-like object `output_fd`.

    :param objs: A tree of objects (metrics and pings) as returned from
    `parser.parse_objects`.
    :param output_fd: Writeable file to write the output to.
    :param options: options dictionary.
    """

    # Monkeypatch util.get_jinja2_template to find templates nearby

    def get_local_template(template_name, filters=()):
        env = jinja2.Environment(
            loader=jinja2.PackageLoader("cpp", "templates"),
            trim_blocks=True,
            lstrip_blocks=True,
        )
        env.filters["camelize"] = util.camelize
        env.filters["Camelize"] = util.Camelize
        for filter_name, filter_func in filters:
            env.filters[filter_name] = filter_func
        return env.get_template(template_name)

    util.get_jinja2_template = get_local_template

    if "pings" in objs:
        template = util.get_jinja2_template(
            "cpp_pings.jinja2",
            filters=(("ping_id", generate_ping_ids(objs)),),
        )
        if objs.get("tags"):
            del objs["tags"]
    else:
        template = get_metrics_template(
            options["get_metric_id"]
            if "get_metric_id" in options
            else generate_metric_ids(objs, options)
        )
        objs = get_metrics(objs)

    output_fd.write(template.render(all_objs=objs, options=options))
    output_fd.write("\n")
