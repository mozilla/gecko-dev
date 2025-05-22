# META: timeout=long

from copy import deepcopy

import pytest
from tests.bidi.browsing_context.navigate import navigate_and_assert

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize(
    "accept_insecure_certs_in_session",
    [True, False],
    ids=["True_in_session", "False_in_session"],
)
@pytest.mark.parametrize(
    "accept_insecure_certs_in_user_context",
    [True, False],
    ids=["True_in_user_context", "False_in_user_context"],
)
async def test_accept_insecure_certs(
    configuration,
    url,
    create_custom_profile,
    geckodriver,
    accept_insecure_certs_in_session,
    accept_insecure_certs_in_user_context,
):
    # Create a fresh profile without any item in the certificate storage so that
    # loading a HTTPS page will cause an insecure certificate error
    custom_profile = create_custom_profile(clone=False)

    config = deepcopy(configuration)
    config["capabilities"]["moz:firefoxOptions"]["args"] = [
        "--profile",
        custom_profile.profile,
    ]
    # Capability matching not implemented yet for WebDriver BiDi (bug 1713784)
    config["capabilities"]["webSocketUrl"] = True
    config["capabilities"]["acceptInsecureCerts"] = accept_insecure_certs_in_session

    driver = geckodriver(config=config)
    driver.new_session()

    bidi_session = driver.session.bidi_session
    await bidi_session.start()

    user_context = await bidi_session.browser.create_user_context(
        accept_insecure_certs=accept_insecure_certs_in_user_context
    )
    new_context = await bidi_session.browsing_context.create(
        user_context=user_context, type_hint="tab"
    )

    # Make sure that "acceptInsecureCerts" from the user context is applied.
    await navigate_and_assert(
        bidi_session,
        new_context,
        url("/common/blank.html", protocol="https"),
        expected_error=not accept_insecure_certs_in_user_context,
    )

    # Create a tab in the default user context
    new_context = await bidi_session.browsing_context.create(
        user_context="default", type_hint="tab"
    )

    # Make sure that "acceptInsecureCerts" from the session is applied.
    await navigate_and_assert(
        bidi_session,
        new_context,
        url("/common/blank.html", protocol="https"),
        expected_error=not accept_insecure_certs_in_session,
    )

    await driver.delete_session()
