# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT


# based on bug 1832141
class CUSTOM_PLURALS(PLURALS):
    def __call__(self, ctx):
        pattern = super().__call__(ctx)
        el = pattern.elements[0]
        if isinstance(el, FTL.Placeable) and isinstance(
            el.expression, FTL.SelectExpression
        ):
            selexp = el.expression
        else:
            selexp = FTL.SelectExpression(
                VARIABLE_REFERENCE("requestCount"),
                [FTL.Variant(FTL.Identifier("other"), pattern, default=True)],
            )
            pattern = FTL.Pattern([FTL.Placeable(selexp)])
        el.expression.variants[0:0] = [
            FTL.Variant(
                FTL.NumberLiteral("0"),
                COPY(self.path, "networkMenu.summary.requestsCountEmpty")(ctx),
            )
        ]
        return pattern


def migrate(ctx):
    """Bug 1953339 - Migrate netmonitor statusbar strings to fluent, part {index}."""

    source = "devtools/client/netmonitor.properties"
    target = "devtools/client/netmonitor.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-tooltip-perf"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "networkMenu.summary.tooltip.perf"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-tooltip-domcontentloaded"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=REPLACE(
                            source,
                            "networkMenu.summary.tooltip.domContentLoaded",
                            {"DOMContentLoad": FTL.TextElement("DOMContentLoaded")},
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-tooltip-load"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "networkMenu.summary.tooltip.load"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-requests-count"),
                value=CUSTOM_PLURALS(
                    source,
                    "networkMenu.summary.requestsCount2",
                    VARIABLE_REFERENCE("requestCount"),
                    lambda text: REPLACE_IN_TEXT(
                        text, {"#1": VARIABLE_REFERENCE("requestCount")}
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-tooltip-requests-count"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "networkMenu.summary.tooltip.requestsCount"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-transferred"),
                value=REPLACE(
                    source,
                    "networkMenu.summary.transferred",
                    {
                        "%1$S": VARIABLE_REFERENCE("formattedContentSize"),
                        "%2$S": VARIABLE_REFERENCE("formattedTransferredSize"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-tooltip-transferred"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "networkMenu.summary.tooltip.transferred"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-finish"),
                value=REPLACE(
                    source,
                    "networkMenu.summary.finish",
                    {"%1$S": VARIABLE_REFERENCE("formattedTime")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("network-menu-summary-tooltip-finish"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "networkMenu.summary.tooltip.finish"),
                    ),
                ],
            ),
        ],
    )
