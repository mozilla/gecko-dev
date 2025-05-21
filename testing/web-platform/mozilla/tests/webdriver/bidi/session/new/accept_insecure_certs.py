from copy import deepcopy

import pytest
from tests.bidi.browsing_context.navigate import navigate_and_assert

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("accept_insecure_certs", [True, False])
async def test_accept_insecure_certs(
    configuration, url, create_custom_profile, geckodriver, accept_insecure_certs
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
    config["capabilities"]["acceptInsecureCerts"] = accept_insecure_certs
    config["capabilities"]["webSocketUrl"] = True

    driver = geckodriver(config=config)
    driver.new_session()

    bidi_session = driver.session.bidi_session
    await bidi_session.start()

    contexts = await bidi_session.browsing_context.get_tree(max_depth=0)

    await navigate_and_assert(
        bidi_session,
        contexts[0],
        url("/common/blank.html", protocol="https"),
        expected_error=not accept_insecure_certs,
    )

    await driver.delete_session()
