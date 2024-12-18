import asyncio

import pytest

URL = "https://flipbook.se.com/fr/fr/fraed219034fr/04-2024/#page/1053"

PAGE_CSS = ".page-wrapper"
ZOOMER_CSS = ".zoomer, .book.zoom-in"


async def check_if_esc_zooms_out(client):
    await client.navigate(URL)

    # If we double-click too early, it can break the page.
    await asyncio.sleep(1)

    # Double-click until the zoomer element appears. Note that the markup is
    # differs with the interventions off, so ZOOMER_CSS has two values.
    zoomer = None
    for i in range(10):
        await client.apz_click(coords=(400, 400))
        await asyncio.sleep(0.1)
        await client.apz_click(coords=(400, 400))
        await asyncio.sleep(0.5)
        zoomer = client.find_css(ZOOMER_CSS)
        if zoomer:
            break
    assert zoomer

    # Now compare the zoom-level to its value after we press ESC.
    zoomed_in = client.execute_script("return arguments[0].style.transform", zoomer)
    client.await_css("body").send_keys("\ue00c")  # Escape key
    asyncio.sleep(0.5)
    return zoomed_in != client.execute_script(
        "return arguments[0].style.transform", zoomer
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_if_esc_zooms_out(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_if_esc_zooms_out(client)
