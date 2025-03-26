# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1955586 - Convert three browser.properties used in ToolbarDropHandler.sys.mjs to Fluent, part {index}."""

    source = "browser/chrome/browser/browser.properties"
    target = "browser/browser/toolbarDropHandler.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("toolbar-drop-on-home-title"),
                value=COPY(source, "droponhometitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbar-drop-on-home-msg"),
                value=COPY(source, "droponhomemsg"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbar-drop-on-home-msg-multiple"),
                value=COPY(source, "droponhomemsgMultiple"),
            ),
        ],
    )
