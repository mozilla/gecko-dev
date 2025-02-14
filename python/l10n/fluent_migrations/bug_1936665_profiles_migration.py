# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1936665 - Migrate profiles fluent strings, part {index}."""

    ctx.add_transforms(
        "browser/browser/profiles.ftl",
        "browser/browser/profiles.ftl",
        transforms_from(
            """
edit-profile-page-theme-header-2 =
    .label = {COPY_PATTERN(from_path, "edit-profile-page-theme-header")}

edit-profile-page-avatar-header-2 =
    .label = {COPY_PATTERN(from_path, "edit-profile-page-avatar-header")}

book-avatar = {COPY_PATTERN(from_path, "book-avatar-alt.alt")}
briefcase-avatar = {COPY_PATTERN(from_path, "briefcase-avatar-alt.alt")}
flower-avatar = {COPY_PATTERN(from_path, "flower-avatar-alt.alt")}
heart-avatar = {COPY_PATTERN(from_path, "heart-avatar-alt.alt")}
shopping-avatar = {COPY_PATTERN(from_path, "shopping-avatar-alt.alt")}
star-avatar = {COPY_PATTERN(from_path, "star-avatar-alt.alt")}
    """,
            from_path="browser/browser/profiles.ftl",
        ),
    )
