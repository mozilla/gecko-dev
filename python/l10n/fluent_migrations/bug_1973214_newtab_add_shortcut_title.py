# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1973214 - Migrate add shortcut tooltip, part {index}"""

    source = "browser/browser/newtab/newtab.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
newtab-topsites-add-shortcut-title =
    .title = {COPY_PATTERN(from_path, "newtab-topsites-add-shortcut-label")}
    .aria-label = {COPY_PATTERN(from_path, "newtab-topsites-add-shortcut-label")}
""",
            from_path=source,
        ),
    )
