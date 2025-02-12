import pytest

URL = "https://www.aliexpress.com/"
SHIP_TO_CSS = "[class*=ship-to--menuItem--]"
CURRENT_FLAG_CSS = "[class*=ship-to--menuItem--] [class*=country-flag-]"


async def is_flag_correct_height(client):
    await client.navigate(URL)
    client.await_css(SHIP_TO_CSS, is_displayed=True).click()
    flag = client.await_css(CURRENT_FLAG_CSS, is_displayed=True)
    return client.execute_script(
        """
        const flag = arguments[0];
        const style = getComputedStyle(flag);
        return parseFloat(style.height) * parseFloat(style.zoom) == flag.getBoundingClientRect().height
      """,
        flag,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_flag_correct_height(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_flag_correct_height(client)
