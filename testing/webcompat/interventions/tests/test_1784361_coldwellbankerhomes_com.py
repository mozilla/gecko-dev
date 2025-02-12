import pytest

URL = "https://www.coldwellbankerhomes.com/ri/little-compton/kvc-17_1,17_2/"
ANDROID_ERROR_MSG = 'can\'t access property "dataset", v[0] is undefined'
LOADED_IMG_CSS = "img.psr-lazy:not([src*='spacer'])"


async def get_image_aspect_ratio(client):
    await client.navigate(URL)
    img = client.await_css(LOADED_IMG_CSS)
    return client.execute_script(
        """
       const img = arguments[0];
       return img.clientWidth / img.clientHeight;
    """,
        img,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await get_image_aspect_ratio(client) < 1.6


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled_desktop(client):
    assert not await get_image_aspect_ratio(client) > 1.6


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled_android(client):
    await client.navigate(URL, await_console_message=ANDROID_ERROR_MSG)
