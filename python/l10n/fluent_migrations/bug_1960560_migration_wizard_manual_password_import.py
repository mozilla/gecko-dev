# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1960560 - Generalize IDs of strings for migration wizard manual password import, part {index}."""

    source = "browser/browser/migrationWizard.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
migration-manual-password-import-skip-button = {COPY_PATTERN(from_path, "migration-safari-password-import-skip-button")}
migration-manual-password-import-select-button = {COPY_PATTERN(from_path, "migration-safari-password-import-select-button")}
""",
            from_path=source,
        ),
    )
