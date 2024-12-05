# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1935504 - Update weather widget location search input placeholder string to be translated, part {index}"""

    source = "browser/browser/newtab/newtab.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
newtab-weather-change-location-search-input-placeholder =
    .placeholder = {COPY_PATTERN(from_path, "newtab-weather-change-location-search-input")}
    .aria-label = {COPY_PATTERN(from_path, "newtab-weather-change-location-search-input")}
""",
            from_path=source,
        ),
    )
