import pytest

URL = "https://stream.directv.com/"
LOGIN_CSS = "#userID"
UNSUPPORTED_CSS = ".title-new-browse-ff"
DENIED_XPATH = "//h1[text()='Access Denied']"
BLOCKED_TEXT = "An error occurred while processing your request."


async def check_site(client, should_pass):
    await client.navigate(URL)

    denied, blocked, login, unsupported = client.await_first_element_of(
        [
            client.xpath(DENIED_XPATH),
            client.text(BLOCKED_TEXT),
            client.css(LOGIN_CSS),
            client.css(UNSUPPORTED_CSS),
        ],
        is_displayed=True,
    )

    if blocked:
        pytest.skip("Blocked from accessing site. Please try testing manually.")
        return

    if denied:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to USA.")
        return

    assert (should_pass and login) or (not should_pass and unsupported)


@pytest.mark.asyncio
@pytest.mark.with_interventions
@pytest.mark.skip_platforms("android")
async def test_enabled(client):
    await check_site(client, should_pass=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
@pytest.mark.skip_platforms("android")
async def test_disabled(client):
    await check_site(client, should_pass=False)
