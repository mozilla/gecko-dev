# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.transforms import COPY, LegacySource, Transform


class REMOVE_EXTENSION(LegacySource):
    def __call__(self, ctx):
        element: FTL.TextElement = super(REMOVE_EXTENSION, self).__call__(ctx)
        element.value = element.value.removesuffix(".bmp")
        return Transform.pattern_of(element)


def migrate(ctx):
    """Bug 1955747 - Migrate shellservice.properties to fluent, part {index}."""

    source = "browser/chrome/browser/shellservice.properties"
    target = "browser/browser/setDesktopBackground.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
set-desktop-background-downloading =
    .label = { COPY(from_path, "DesktopBackgroundDownloading") }
    """,
            from_path=source,
        ),
    )

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("set-desktop-background-filename"),
                value=REMOVE_EXTENSION(
                    source,
                    "desktopBackgroundLeafNameWin",
                ),
            ),
        ],
    )
