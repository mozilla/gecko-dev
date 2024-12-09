# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate import COPY_PATTERN
from fluent.migrate.transforms import TransformPattern
from fluent.migrate.transforms import COPY


class ADD_SHORTCUT(TransformPattern):
    def visit_TextElement(self, node):
        node.value = f"{node.value} ({{ $shortcut }})"
        return node


def migrate(ctx):
    """Bug 1897411 - Add keyboard shortcut for expanding the sidebar, part {index}."""

    properties_file = (
        "browser/chrome/browser/customizableui/customizableWidgets.properties"
    )
    ftl_file = "browser/browser/sidebar.ftl"
    ctx.add_transforms(
        ftl_file,
        ftl_file,
        [
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-expand-sidebar2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=ADD_SHORTCUT(
                            ftl_file, "sidebar-widget-expand-sidebar.tooltiptext"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(properties_file, "sidebar-button.label"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-collapse-sidebar2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=ADD_SHORTCUT(
                            ftl_file, "sidebar-widget-collapse-sidebar.tooltiptext"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(properties_file, "sidebar-button.label"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-show-sidebar2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=ADD_SHORTCUT(
                            ftl_file, "sidebar-widget-show-sidebar.tooltiptext"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(properties_file, "sidebar-button.label"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("sidebar-widget-hide-sidebar2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=ADD_SHORTCUT(
                            ftl_file, "sidebar-widget-hide-sidebar.tooltiptext"
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
