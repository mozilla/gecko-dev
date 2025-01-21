# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1921325 - Update structure of new tab toast notification message for moz-message-bar, part {index}."""

    source = "browser/browser/newtab/newtab.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
newtab-toast-thumbs-up-or-down2 =
    .message = {COPY_PATTERN(from_path, "newtab-toast-thumbs-up-or-down")}
""",
            from_path=source,
        ),
    )
