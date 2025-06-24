import pytest

URL = "https://www.wills-vegan-store.com/collections/vegan-shoes-womens"
WRAPPER_CSS = ".media-wrapper :has(img.motion-reduce)"


async def are_product_images_visible(client):
    await client.navigate(URL, wait="none")
    wrapper = client.await_css(WRAPPER_CSS, is_displayed=True)
    return client.execute_script(
        "return arguments[0].getBoundingClientRect().width > 0", wrapper
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await are_product_images_visible(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await are_product_images_visible(client)
