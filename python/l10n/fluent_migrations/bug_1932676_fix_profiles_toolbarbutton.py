# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1932676 - Fix localization of profiles button, part {index}"""

    source = "browser/browser/appmenu.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
appmenu-profiles-2 =
    .label = {COPY_PATTERN(from_path, "appmenu-profiles")}
""",
            from_path=source,
        ),
    )
