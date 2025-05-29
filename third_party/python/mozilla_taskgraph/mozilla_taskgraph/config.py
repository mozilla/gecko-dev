from textwrap import dedent

from taskgraph import config as tg
from voluptuous import Optional

tg.graph_config_schema = tg.graph_config_schema.extend(
    {
        Optional("shipit"): {
            Optional("product"): str,
            Optional("release-format"): str,
            Optional("scope-prefix"): str,
        },
        Optional(
            "version-parser",
            description=dedent(
                """
                Python path of the form ``<module>:<obj>`` pointing to a
                function that takes a set of parameters as input and returns
                the version string to use for release tasks.

                Defaults to ``mozilla_taskgraph.version:default_parser``.
                    """.lstrip()
            ),
        ): str,
    }
)
