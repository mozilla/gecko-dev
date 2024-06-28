# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1884970 - Close current tab button is missing an accessible name and role, part {index}."""

    source = "browser/browser/tabbrowser.ftl"
    target = "browser/browser/tabbrowser.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
tabbrowser-close-tabs-button =
    .tooltiptext = {COPY_PATTERN(from_path, "tabbrowser-close-tabs-tooltip.label")}
""",
            from_path=source,
        ),
    )
