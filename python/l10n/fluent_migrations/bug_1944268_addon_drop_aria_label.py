# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1944268 - Drop redundant aria-label, part {index}."""

    source = "toolkit/toolkit/about/aboutAddons.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
addon-badge-private-browsing-allowed3 =
    .title = {COPY_PATTERN(from_path, "addon-badge-private-browsing-allowed2.title")}
addon-badge-recommended3 =
    .title = {COPY_PATTERN(from_path, "addon-badge-recommended2.title")}
addon-badge-line4 =
    .title = {COPY_PATTERN(from_path, "addon-badge-line3.title")}
addon-badge-verified3 =
    .title = {COPY_PATTERN(from_path, "addon-badge-verified2.title")}
""",
            from_path=source,
        ),
    )
