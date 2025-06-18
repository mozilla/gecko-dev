import pytest
from webdriver.error import ElementClickInterceptedException

URL = "https://www.buzzfeed.com/trendyelephant793/thanksgiving-foods-showdown-quiz"
GOOGLE_LOGIN_POPUP_CSS = "#credential_picker_iframe"
START_BUTTON = "[class*=introWrapper] button"
CARD_FRONT_CSS = "[class*=isFlipped] [class*=cardFace][class*=cardFront]"


async def can_click_on_cards(client):
    expected_popups = [client.css(GOOGLE_LOGIN_POPUP_CSS)]
    await client.navigate(URL)
    start = client.await_css(START_BUTTON, is_displayed=True)
    client.scroll_into_view(start)
    start.click()
    front = client.await_css(CARD_FRONT_CSS, is_displayed=True)
    client.execute_async_script(
        """
      const [card, ready] = arguments;
      card.parentElement.addEventListener("transitionend", ready);
    """,
        front,
    )
    try:
        client.click(front, popups=expected_popups)
    except ElementClickInterceptedException:
        return False
    return True


@pytest.mark.only_firefox_versions(max=140)
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_click_on_cards(client)


@pytest.mark.only_firefox_versions(max=140)
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_click_on_cards(client)


@pytest.mark.only_firefox_versions(min=141)
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_regression(client):
    assert await can_click_on_cards(client)
