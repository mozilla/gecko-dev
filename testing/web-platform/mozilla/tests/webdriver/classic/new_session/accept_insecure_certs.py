from copy import deepcopy

import pytest
from tests.support.asserts import assert_error, assert_success

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
    config["capabilities"]["pageLoadStrategy"] = "eager"
    config["capabilities"]["acceptInsecureCerts"] = accept_insecure_certs

    driver = geckodriver(config=config)
    driver.new_session()

    session = driver.session

    page = url("/common/blank.html", protocol="https")

    response = session.transport.send(
        "POST",
        f"session/{session.session_id}/url",
        {"url": page},
        headers={"content-type": "application/json"},
    )

    if accept_insecure_certs:
        assert_success(response)
    else:
        assert_error(response, "insecure certificate")

    await driver.delete_session()
