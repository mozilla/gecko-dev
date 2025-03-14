# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1953138 - Migrate insecure form strings from .properties to fluent, part {index}."""

    source = "toolkit/chrome/global/browser.properties"
    target = "toolkit/toolkit/global/htmlForm.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("form-post-secure-to-insecure-warning-title"),
                value=COPY(source, "formPostSecureToInsecureWarning.title"),
            ),
            FTL.Message(
                id=FTL.Identifier("form-post-secure-to-insecure-warning-message"),
                value=COPY(source, "formPostSecureToInsecureWarning.message"),
            ),
            FTL.Message(
                id=FTL.Identifier("form-post-secure-to-insecure-warning-continue"),
                value=COPY(source, "formPostSecureToInsecureWarning.continue"),
            ),
        ],
    )
