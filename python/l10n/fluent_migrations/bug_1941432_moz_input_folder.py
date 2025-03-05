# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1941432 - Create a basic moz-input-file element, part {index}."""

    source = "browser/browser/preferences/preferences.ftl"
    target = "toolkit/toolkit/global/mozInputFolder.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
choose-folder-button =
    .label = {COPY_PATTERN(from_path, "download-choose-folder.label")}
    .accesskey = {COPY_PATTERN(from_path, "download-choose-folder.accesskey ")}
""",
            from_path=source,
        ),
    )
