# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Outputter to generate TypeScript declarations for metrics and pings.
"""

import jinja2
from glean_parser import util
from util import get_metrics


def enum(variants):
    return '"' + '"|"'.join(sorted(variants)) + '"'


def metric(obj):
    if obj.type.startswith("labeled_"):
        labels = enum(obj.labels) if obj.labels else "string"
        return f"Record<{labels}, Glean{util.Camelize(obj.type[8:])}>"

    if obj.type == "event":
        if not obj.allowed_extra_keys:
            return "GleanEventNoExtras"

        # Though we know the types of the event extras, the webidl signature
        # of GleanEvent.record requires us to treat them all as strings.
        props = [f"{key}?: string" for key in obj.allowed_extra_keys]
        return f"GleanEventWithExtras<{{ {', '.join(props)} }}>"

    return "Glean" + util.Camelize(obj.type)


def ping(obj):
    if not obj.reason_codes:
        return "GleanPingNoReason"

    return f"GleanPingWithReason<{enum(obj.reason_codes)}>"


def output_dts(objs, output_fd):
    """
    :param objs: A tree of metrics and pings as returned from `parser.parse_objects`.
    """

    loader = jinja2.PackageLoader("typescript", "templates")
    env = jinja2.Environment(loader=loader, trim_blocks=True, lstrip_blocks=True)
    env.filters["camelize"] = util.camelize
    env.filters["metric"] = metric
    env.filters["ping"] = ping

    template = env.get_template("typescript.jinja2")
    output_fd.write(template.render(metrics=get_metrics(objs), pings=objs["pings"]))
    output_fd.write("\n")
