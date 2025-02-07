import pytest

# The gallery on this page only sets img[src] to a non-1x1-spacer if the error
# doesn't happen during page load, which doesn't happen with a Chrome UA.

URL = "https://www.coldwellbankerhomes.com/ri/little-compton/kvc-17_1,17_2/"
ERROR_MSG = 'can\'t access property "dataset", v[0] is undefined'
LOADED_IMG_CSS = "img.psr-lazy:not([src*='spacer'])"


async def is_image_too_tall(client):
    await client.navigate(URL)
    img = client.await_css(LOADED_IMG_CSS)
    return client.execute_script(
        """
       const img = arguments[0];
       const container = img.closest("div");
       return img.clientHeight > container.clientHeight + 10;
    """,
        img,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_image_too_tall(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_image_too_tall(client)
