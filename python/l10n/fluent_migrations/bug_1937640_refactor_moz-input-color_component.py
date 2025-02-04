# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1937640 - Refactor moz-input-color component, part {index}."""

    source = "toolkit/toolkit/about/aboutReader.ftl"
    target = source

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
about-reader-custom-colors-foreground2 =
    .label = {COPY_PATTERN(from_path, "about-reader-custom-colors-foreground")}
    .title = Edit color
about-reader-custom-colors-background2 =
    .label = {COPY_PATTERN(from_path, "about-reader-custom-colors-background")}
    .title = Edit color

about-reader-custom-colors-unvisited-links2 =
    .label = {COPY_PATTERN(from_path, "about-reader-custom-colors-unvisited-links")}
    .title = Edit color
about-reader-custom-colors-visited-links2 =
    .label = {COPY_PATTERN(from_path, "about-reader-custom-colors-visited-links")}
    .title = Edit color
about-reader-custom-colors-selection-highlight2 =
    .label = {COPY_PATTERN(from_path, "about-reader-custom-colors-selection-highlight")}
    .title = Edit color
""",
            from_path=source,
        ),
    )
