# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate import COPY_PATTERN
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1921539 - Sidebar button has no label when moved to the overflow menu, part {index}."""

    properties_file = (
        "browser/chrome/browser/customizableui/customizableWidgets.properties"
    )
    ftl_file = "browser/browser/sidebar.ftl"
    ctx.add_transforms(
        ftl_file,
        ftl_file,
        [
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-expand-sidebar"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY_PATTERN(
                            ftl_file, "sidebar-toolbar-expand-sidebar.tooltiptext"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(properties_file, "sidebar-button.label"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-collapse-sidebar"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY_PATTERN(
                            ftl_file, "sidebar-toolbar-collapse-sidebar.tooltiptext"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(properties_file, "sidebar-button.label"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-show-sidebar"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY_PATTERN(
                            ftl_file, "sidebar-toolbar-show-sidebar.tooltiptext"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(properties_file, "sidebar-button.label"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-hide-sidebar"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY_PATTERN(
                            ftl_file, "sidebar-toolbar-hide-sidebar.tooltiptext"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(properties_file, "sidebar-button.label"),
                    ),
                ],
            ),
        ],
    )
