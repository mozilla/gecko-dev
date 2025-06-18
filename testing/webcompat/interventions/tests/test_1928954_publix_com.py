import asyncio

import pytest

URL = "https://www.publix.com/"

HERO_CSS = ".widget.banner.standard"
LINKS_CSS = "a[href^='javascript:']"
POPUP_CLOSE_BUTTON_CSS = ".external-site-popup #stay-button"


async def does_click_navigate_away(client):
    await client.make_preload_script(
        "navigator.geolocation.getCurrentPosition = () => {}"
    )
    await client.navigate(URL, wait="none")
    client.await_css(HERO_CSS, is_displayed=True)
    links = client.find_css(LINKS_CSS, is_displayed=True, all=True)
    assert len(links) > 0
    for link in links:
        client.soft_click(link)
        await asyncio.sleep(0.5)
        if not client.find_css(HERO_CSS):  # page is replaced on failure
            return True
        client.await_css(POPUP_CLOSE_BUTTON_CSS, is_displayed=True).click()
        client.await_element_hidden(client.css(POPUP_CLOSE_BUTTON_CSS))
    return False


@pytest.mark.only_firefox_versions(max=139)
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await does_click_navigate_away(client)


@pytest.mark.only_firefox_versions(max=139)
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await does_click_navigate_away(client)


@pytest.mark.only_firefox_versions(min=140)
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_regression(client):
    assert not await does_click_navigate_away(client)
