import pytest
from webdriver.error import ElementClickInterceptedException

URL = "https://nppes.cms.hhs.gov/#/"

BUTTON_CSS = "button[ng-click='accept()']"


async def can_click_accept(client):
    await client.navigate(URL)
    try:
        client.await_css(BUTTON_CSS, is_displayed=True).click()
    except ElementClickInterceptedException:
        return False
    return True


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_click_accept(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_click_accept(client)
