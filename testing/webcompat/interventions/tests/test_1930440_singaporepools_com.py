import pytest

URL = "https://online.singaporepools.com/en/lottery"

UNSUPPORTED_ALERT = "unsupported browser"
LOGIN_CSS = ".mini-login-gadget"


async def do_checks(client, shouldPass):
    if not shouldPass:
        alert = await client.await_alert(UNSUPPORTED_ALERT, timeout=10)

    await client.navigate(URL)

    if not shouldPass:
        assert await alert

    # the login bits of the page should properly load
    assert client.await_css(LOGIN_CSS)

    # we should never see the in-page support warning
    warningFound = client.execute_script(
        """
      for (const warning of document.querySelectorAll(".alert.views-row")) {
        if (warning.innerText.includes("Chrome")) {
          return true;
        }
      }
      return false;
    """
    )
    assert (shouldPass and not warningFound) or (not shouldPass and warningFound)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await do_checks(client, True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await do_checks(client, False)
