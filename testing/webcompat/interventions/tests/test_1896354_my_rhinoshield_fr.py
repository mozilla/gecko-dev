import pytest
from webdriver.error import NoSuchElementException

URL = "https://my.rhinoshield.fr"
BLOCKED_TEXT = "Veuillez utiliser Google Chrome"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="load")
    try:
        client.await_text(BLOCKED_TEXT, is_displayed=True, timeout=5)
        assert False
    except NoSuchElementException:
        assert True


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(BLOCKED_TEXT)
