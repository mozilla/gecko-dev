#  Any copyright is dedicated to the Public Domain.
#  http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1919598 - Land Firefox View Discoverability treatment-b in mc-, part {index}."""

    target = "browser/browser/featureCallout.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
split-dismiss-button-dont-show-option =
    .label = {COPY_PATTERN(from_path, "split-dismiss-button-dont-show-option-label")}

split-dismiss-button-show-fewer-option =
    .label = {COPY_PATTERN(from_path, "split-dismiss-button-show-fewer-option-label")}

split-dismiss-button-manage-settings-option =
    .label = {COPY_PATTERN(from_path, "split-dismiss-button-manage-settings-option-label")}
""",
            from_path=target,
        ),
    )
