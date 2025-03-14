# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.transforms import REPLACE


def migrate(ctx):
    """Bug 1948916 - Bundle formautofill into Firefox Desktop omni jar - part {index}."""

    propertiesSource = "browser/extensions/formautofill/formautofill.properties"
    target = "toolkit/toolkit/formautofill/formAutofill.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("autofill-insecure-field-warning-description"),
                value=REPLACE(
                    propertiesSource,
                    "insecureFieldWarningDescription",
                    {
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
        ],
    )
