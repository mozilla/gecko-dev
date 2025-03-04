# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1948326 - Move addEngine dialog from preferences to search component, part {index}."""

    source = "browser/browser/preferences/addEngine.ftl"
    target = "browser/browser/search.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
add-engine-window =
    .title = {COPY_PATTERN(from_path, "add-engine-window2.title")}
    .style = {COPY_PATTERN(from_path, "add-engine-window2.style")}

add-engine-button = {COPY_PATTERN(from_path, "add-engine-button")}

add-engine-name = {COPY_PATTERN(from_path, "add-engine-name")}

add-engine-url = {COPY_PATTERN(from_path, "add-engine-url")}

add-engine-dialog =
    .buttonlabelaccept = {COPY_PATTERN(from_path, "add-engine-dialog.buttonlabelaccept")}
    .buttonaccesskeyaccept = {COPY_PATTERN(from_path, "add-engine-dialog.buttonaccesskeyaccept")}

engine-name-exists = {COPY_PATTERN(from_path, "engine-name-exists")}
""",
            from_path=source,
        ),
    )
