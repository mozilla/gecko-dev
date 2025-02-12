# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1944420 - Migrate to use the label for menuitem, part {index}."""

    source = "browser/browser/browser.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
urlbar-searchmode-popup-search-settings-menuitem =
    .label = {COPY_PATTERN(from_path, "urlbar-searchmode-popup-search-settings")}
""",
            from_path=source,
        ),
    )
